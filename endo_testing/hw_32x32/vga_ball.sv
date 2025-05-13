/*
 * Avalon memory-mapped peripheral for VGA Ball Game
 * Modified for 32×32 sprites, 8×4k-line ROMs, 256-color (8‑bit) pixels
 */
module vga_ball #(
    parameter int MAX_OBJECTS  = 100,
    parameter int SPRITE_WIDTH  = 32,
    parameter int SPRITE_HEIGHT = 32
) (
    input  logic        clk,
    input  logic        reset,
    input  logic [31:0] writedata,
    input  logic        write,
    input  logic        chipselect,
    input  logic [6:0]  address,

    output logic [7:0]  VGA_R, VGA_G, VGA_B,
    output logic        VGA_CLK,
    output logic        VGA_HS,
    output logic        VGA_VS,
    output logic        VGA_BLANK_n,
    output logic        VGA_SYNC_n
);

    // VGA counters
    logic [10:0] hcount;
    logic [9:0]  vcount;
    vga_counters counters(
        .clk50(clk),
        .*  // connects hcount, vcount, VGA_HS, VGA_VS, VGA_CLK, VGA_BLANK_n, VGA_SYNC_n
    );

    // Background color
    logic [7:0] background_r, background_g, background_b;

    // Object state arrays
    logic [11:0] obj_x      [MAX_OBJECTS];
    logic [11:0] obj_y      [MAX_OBJECTS];
    logic [4:0]  obj_sprite [MAX_OBJECTS];  // 5-bit pattern ID (0…31)
    logic        obj_active [MAX_OBJECTS];

    // Sprite address computation
    localparam int SPRITE_SIZE = SPRITE_WIDTH * SPRITE_HEIGHT;  // 1024
    logic [14:0] sprite_address;      // total: 8×4096 = 32768 entries (15 bits)
    wire  [2:0]  sprite_rom_sel  = sprite_address[14:12];
    wire  [11:0] sprite_rom_addr = sprite_address[11:0];

    // Relative pixel coordinates (0…31)
    logic [4:0] rel_x, rel_y;

    // ROM data outputs
    wire [7:0] rom_data0, rom_data1, rom_data2, rom_data3;
    wire [7:0] rom_data4, rom_data5, rom_data6, rom_data7;
    wire [7:0] rom_data;

    // Instantiate 8 sprite ROMs (4 k lines × 8 bits)
    soc_system_rom_sprites sprite_images0 (.address(sprite_rom_addr), .chipselect(1'b1), .clk(clk), .clken(1'b1), .debugaccess(1'b0), .freeze(1'b0), .reset(1'b0), .reset_req(1'b0), .write(1'b0), .writedata(32'b0), .readdata(rom_data0));
    soc_system_rom_sprites sprite_images1 (.address(sprite_rom_addr), .chipselect(1'b1), .clk(clk), .clken(1'b1), .debugaccess(1'b0), .freeze(1'b0), .reset(1'b0), .reset_req(1'b0), .write(1'b0), .writedata(32'b0), .readdata(rom_data1));
    soc_system_rom_sprites sprite_images2 (.address(sprite_rom_addr), .chipselect(1'b1), .clk(clk), .clken(1'b1), .debugaccess(1'b0), .freeze(1'b0), .reset(1'b0), .reset_req(1'b0), .write(1'b0), .writedata(32'b0), .readdata(rom_data2));
    soc_system_rom_sprites sprite_images3 (.address(sprite_rom_addr), .chipselect(1'b1), .clk(clk), .clken(1'b1), .debugaccess(1'b0), .freeze(1'b0), .reset(1'b0), .reset_req(1'b0), .write(1'b0), .writedata(32'b0), .readdata(rom_data3));
    soc_system_rom_sprites sprite_images4 (.address(sprite_rom_addr), .chipselect(1'b1), .clk(clk), .clken(1'b1), .debugaccess(1'b0), .freeze(1'b0), .reset(1'b0), .reset_req(1'b0), .write(1'b0), .writedata(32'b0), .readdata(rom_data4));
    soc_system_rom_sprites sprite_images5 (.address(sprite_rom_addr), .chipselect(1'b1), .clk(clk), .clken(1'b1), .debugaccess(1'b0), .freeze(1'b0), .reset(1'b0), .reset_req(1'b0), .write(1'b0), .writedata(32'b0), .readdata(rom_data5));
    soc_system_rom_sprites sprite_images6 (.address(sprite_rom_addr), .chipselect(1'b1), .clk(clk), .clken(1'b1), .debugaccess(1'b0), .freeze(1'b0), .reset(1'b0), .reset_req(1'b0), .write(1'b0), .writedata(32'b0), .readdata(rom_data6));
    soc_system_rom_sprites sprite_images7 (.address(sprite_rom_addr), .chipselect(1'b1), .clk(clk), .clken(1'b1), .debugaccess(1'b0), .freeze(1'b0), .reset(1'b0), .reset_req(1'b0), .write(1'b0), .writedata(32'b0), .readdata(rom_data7));

    // ROM select mux
    assign rom_data = (sprite_rom_sel == 3'd0) ? rom_data0 :
                      (sprite_rom_sel == 3'd1) ? rom_data1 :
                      (sprite_rom_sel == 3'd2) ? rom_data2 :
                      (sprite_rom_sel == 3'd3) ? rom_data3 :
                      (sprite_rom_sel == 3'd4) ? rom_data4 :
                      (sprite_rom_sel == 3'd5) ? rom_data5 :
                      (sprite_rom_sel == 3'd6) ? rom_data6 : rom_data7;

    // Color palette
    logic [23:0] color_data;
    color_palette palette_inst (
        .clk        (clk),
        .clken      (1'b1),
        .address    (rom_data),
        .color_data (color_data)
    );
    // Final pixel data
    wire [23:0] sprite_data = color_data;

    // Write/update logic
    always_ff @(posedge clk) begin
        if (reset) begin
            background_r <= 8'h00;
            background_g <= 8'h80;
            background_b <= 8'h00;
            for (int i = 0; i < MAX_OBJECTS; i++) begin
                obj_x[i]      <= 12'd0;
                obj_y[i]      <= 12'd0;
                obj_sprite[i] <= 5'd0;
                obj_active[i] <= 1'b0;
            end
        end
        else if (chipselect && write) begin
            if (address == 7'd0) begin
                {background_r, background_g, background_b} <= writedata[23:0];
            end else if (address >= 7'd1 && address < 7'd1 + MAX_OBJECTS) begin
                int obj_idx = address - 7'd1;
                obj_x[obj_idx]      <= writedata[31:20];
                obj_y[obj_idx]      <= writedata[19:8];
                obj_sprite[obj_idx] <= writedata[7:3];
                obj_active[obj_idx] <= writedata[2];
            end
        end
    end

    // Render logic
    logic       found;
    logic [6:0] active_obj_idx;
    logic [23:0] pix, pix_candidate;

    always_comb begin
        found          = 1'b0;
        pix            = {background_r, background_g, background_b};
        sprite_address = 15'd0;

        for (int i = MAX_OBJECTS - 1; i >= 0; i--) begin
            if (!found && obj_active[i] &&
                hcount >= obj_x[i] && hcount < obj_x[i] + SPRITE_WIDTH &&
                vcount >= obj_y[i] && vcount < obj_y[i] + SPRITE_HEIGHT) begin

                rel_x = hcount - obj_x[i];
                rel_y = vcount - obj_y[i];
                sprite_address = obj_sprite[i] * SPRITE_SIZE
                               + rel_y * SPRITE_WIDTH
                               + rel_x;
                pix_candidate = sprite_data;
                if (pix_candidate != 24'h000000) begin
                    pix   = pix_candidate;
                    found = 1'b1;
                end
            end
        end
        {VGA_R, VGA_G, VGA_B} = pix;
    end

endmodule

// VGA timing generator module
module vga_counters(
    input logic        clk50, reset,
    output logic [10:0] hcount,  // hcount是像素列，hcount[10:1]是实际显示的像素位置
    output logic [9:0]  vcount,  // vcount是像素行
    output logic        VGA_CLK, VGA_HS, VGA_VS, VGA_BLANK_n, VGA_SYNC_n
);

    // Parameters for hcount
    parameter HACTIVE      = 11'd 1280,
              HFRONT_PORCH = 11'd 32,
              HSYNC        = 11'd 192,
              HBACK_PORCH  = 11'd 96,   
              HTOTAL       = HACTIVE + HFRONT_PORCH + HSYNC + HBACK_PORCH; // 1600
    
    // Parameters for vcount
    parameter VACTIVE      = 10'd 480,
              VFRONT_PORCH = 10'd 10,
              VSYNC        = 10'd 2,
              VBACK_PORCH  = 10'd 33,
              VTOTAL       = VACTIVE + VFRONT_PORCH + VSYNC + VBACK_PORCH; // 525

    logic endOfLine;
    
    always_ff @(posedge clk50 or posedge reset)
        if (reset)          hcount <= 0;
        else if (endOfLine) hcount <= 0;
        else                hcount <= hcount + 11'd 1;

    assign endOfLine = hcount == HTOTAL - 1;
        
    logic endOfField;
    
    always_ff @(posedge clk50 or posedge reset)
        if (reset)          vcount <= 0;
        else if (endOfLine)
            if (endOfField) vcount <= 0;
            else            vcount <= vcount + 10'd 1;

    assign endOfField = vcount == VTOTAL - 1;

    // Horizontal sync: from 0x520 to 0x5DF (0x57F)
    assign VGA_HS = !( (hcount[10:8] == 3'b101) & !(hcount[7:5] == 3'b111));
    assign VGA_VS = !( vcount[9:1] == (VACTIVE + VFRONT_PORCH) / 2);

    assign VGA_SYNC_n = 1'b0; // For putting sync on the green signal; unused
    
    // Horizontal active: 0 to 1279     Vertical active: 0 to 479
    assign VGA_BLANK_n = !( hcount[10] & (hcount[9] | hcount[8]) ) &
                        !( vcount[9] | (vcount[8:5] == 4'b1111) );

    assign VGA_CLK = hcount[0]; // 25 MHz clock: rising edge sensitive
    
endmodule

module color_palette(
    input  logic        clk,
    input  logic        clken,
    input  logic [7:0]  address,
    output logic [23:0] color_data
);
    always_ff @(posedge clk) begin
        if (clken) begin
            case (address)
                8'h00: color_data <= 24'h000000;
                8'h01: color_data <= 24'h000033;
                8'h02: color_data <= 24'h000066;
                8'h03: color_data <= 24'h000099;
                8'h04: color_data <= 24'h0000CC;
                8'h05: color_data <= 24'h0000FF;
                8'h06: color_data <= 24'h003300;
                8'h07: color_data <= 24'h003333;
                8'h08: color_data <= 24'h003366;
                8'h09: color_data <= 24'h003399;
                8'h0A: color_data <= 24'h0033CC;
                8'h0B: color_data <= 24'h0033FF;
                8'h0C: color_data <= 24'h006600;
                8'h0D: color_data <= 24'h006633;
                8'h0E: color_data <= 24'h006666;
                8'h0F: color_data <= 24'h006699;
                8'h10: color_data <= 24'h0066CC;
                8'h11: color_data <= 24'h0066FF;
                8'h12: color_data <= 24'h009900;
                8'h13: color_data <= 24'h009933;
                8'h14: color_data <= 24'h009966;
                8'h15: color_data <= 24'h009999;
                8'h16: color_data <= 24'h0099CC;
                8'h17: color_data <= 24'h0099FF;
                8'h18: color_data <= 24'h00CC00;
                8'h19: color_data <= 24'h00CC33;
                8'h1A: color_data <= 24'h00CC66;
                8'h1B: color_data <= 24'h00CC99;
                8'h1C: color_data <= 24'h00CCCC;
                8'h1D: color_data <= 24'h00CCFF;
                8'h1E: color_data <= 24'h00FF00;
                8'h1F: color_data <= 24'h00FF33;
                8'h20: color_data <= 24'h00FF66;
                8'h21: color_data <= 24'h00FF99;
                8'h22: color_data <= 24'h00FFCC;
                8'h23: color_data <= 24'h00FFFF;
                8'h24: color_data <= 24'h330000;
                8'h25: color_data <= 24'h330033;
                8'h26: color_data <= 24'h330066;
                8'h27: color_data <= 24'h330099;
                8'h28: color_data <= 24'h3300CC;
                8'h29: color_data <= 24'h3300FF;
                8'h2A: color_data <= 24'h333300;
                8'h2B: color_data <= 24'h333333;
                8'h2C: color_data <= 24'h333366;
                8'h2D: color_data <= 24'h333399;
                8'h2E: color_data <= 24'h3333CC;
                8'h2F: color_data <= 24'h3333FF;
                8'h30: color_data <= 24'h336600;
                8'h31: color_data <= 24'h336633;
                8'h32: color_data <= 24'h336666;
                8'h33: color_data <= 24'h336699;
                8'h34: color_data <= 24'h3366CC;
                8'h35: color_data <= 24'h3366FF;
                8'h36: color_data <= 24'h339900;
                8'h37: color_data <= 24'h339933;
                8'h38: color_data <= 24'h339966;
                8'h39: color_data <= 24'h339999;
                8'h3A: color_data <= 24'h3399CC;
                8'h3B: color_data <= 24'h3399FF;
                8'h3C: color_data <= 24'h33CC00;
                8'h3D: color_data <= 24'h33CC33;
                8'h3E: color_data <= 24'h33CC66;
                8'h3F: color_data <= 24'h33CC99;
                8'h40: color_data <= 24'h33CCCC;
                8'h41: color_data <= 24'h33CCFF;
                8'h42: color_data <= 24'h33FF00;
                8'h43: color_data <= 24'h33FF33;
                8'h44: color_data <= 24'h33FF66;
                8'h45: color_data <= 24'h33FF99;
                8'h46: color_data <= 24'h33FFCC;
                8'h47: color_data <= 24'h33FFFF;
                8'h48: color_data <= 24'h660000;
                8'h49: color_data <= 24'h660033;
                8'h4A: color_data <= 24'h660066;
                8'h4B: color_data <= 24'h660099;
                8'h4C: color_data <= 24'h6600CC;
                8'h4D: color_data <= 24'h6600FF;
                8'h4E: color_data <= 24'h663300;
                8'h4F: color_data <= 24'h663333;
                8'h50: color_data <= 24'h663366;
                8'h51: color_data <= 24'h663399;
                8'h52: color_data <= 24'h6633CC;
                8'h53: color_data <= 24'h6633FF;
                8'h54: color_data <= 24'h666600;
                8'h55: color_data <= 24'h666633;
                8'h56: color_data <= 24'h666666;
                8'h57: color_data <= 24'h666699;
                8'h58: color_data <= 24'h6666CC;
                8'h59: color_data <= 24'h6666FF;
                8'h5A: color_data <= 24'h669900;
                8'h5B: color_data <= 24'h669933;
                8'h5C: color_data <= 24'h669966;
                8'h5D: color_data <= 24'h669999;
                8'h5E: color_data <= 24'h6699CC;
                8'h5F: color_data <= 24'h6699FF;
                8'h60: color_data <= 24'h66CC00;
                8'h61: color_data <= 24'h66CC33;
                8'h62: color_data <= 24'h66CC66;
                8'h63: color_data <= 24'h66CC99;
                8'h64: color_data <= 24'h66CCCC;
                8'h65: color_data <= 24'h66CCFF;
                8'h66: color_data <= 24'h66FF00;
                8'h67: color_data <= 24'h66FF33;
                8'h68: color_data <= 24'h66FF66;
                8'h69: color_data <= 24'h66FF99;
                8'h6A: color_data <= 24'h66FFCC;
                8'h6B: color_data <= 24'h66FFFF;
                8'h6C: color_data <= 24'h990000;
                8'h6D: color_data <= 24'h990033;
                8'h6E: color_data <= 24'h990066;
                8'h6F: color_data <= 24'h990099;
                8'h70: color_data <= 24'h9900CC;
                8'h71: color_data <= 24'h9900FF;
                8'h72: color_data <= 24'h993300;
                8'h73: color_data <= 24'h993333;
                8'h74: color_data <= 24'h993366;
                8'h75: color_data <= 24'h993399;
                8'h76: color_data <= 24'h9933CC;
                8'h77: color_data <= 24'h9933FF;
                8'h78: color_data <= 24'h996600;
                8'h79: color_data <= 24'h996633;
                8'h7A: color_data <= 24'h996666;
                8'h7B: color_data <= 24'h996699;
                8'h7C: color_data <= 24'h9966CC;
                8'h7D: color_data <= 24'h9966FF;
                8'h7E: color_data <= 24'h999900;
                8'h7F: color_data <= 24'h999933;
                8'h80: color_data <= 24'h999966;
                8'h81: color_data <= 24'h999999;
                8'h82: color_data <= 24'h9999CC;
                8'h83: color_data <= 24'h9999FF;
                8'h84: color_data <= 24'h99CC00;
                8'h85: color_data <= 24'h99CC33;
                8'h86: color_data <= 24'h99CC66;
                8'h87: color_data <= 24'h99CC99;
                8'h88: color_data <= 24'h99CCCC;
                8'h89: color_data <= 24'h99CCFF;
                8'h8A: color_data <= 24'h99FF00;
                8'h8B: color_data <= 24'h99FF33;
                8'h8C: color_data <= 24'h99FF66;
                8'h8D: color_data <= 24'h99FF99;
                8'h8E: color_data <= 24'h99FFCC;
                8'h8F: color_data <= 24'h99FFFF;
                8'h90: color_data <= 24'hCC0000;
                8'h91: color_data <= 24'hCC0033;
                8'h92: color_data <= 24'hCC0066;
                8'h93: color_data <= 24'hCC0099;
                8'h94: color_data <= 24'hCC00CC;
                8'h95: color_data <= 24'hCC00FF;
                8'h96: color_data <= 24'hCC3300;
                8'h97: color_data <= 24'hCC3333;
                8'h98: color_data <= 24'hCC3366;
                8'h99: color_data <= 24'hCC3399;
                8'h9A: color_data <= 24'hCC33CC;
                8'h9B: color_data <= 24'hCC33FF;
                8'h9C: color_data <= 24'hCC6600;
                8'h9D: color_data <= 24'hCC6633;
                8'h9E: color_data <= 24'hCC6666;
                8'h9F: color_data <= 24'hCC6699;
                8'hA0: color_data <= 24'hCC66CC;
                8'hA1: color_data <= 24'hCC66FF;
                8'hA2: color_data <= 24'hCC9900;
                8'hA3: color_data <= 24'hCC9933;
                8'hA4: color_data <= 24'hCC9966;
                8'hA5: color_data <= 24'hCC9999;
                8'hA6: color_data <= 24'hCC99CC;
                8'hA7: color_data <= 24'hCC99FF;
                8'hA8: color_data <= 24'hCCCC00;
                8'hA9: color_data <= 24'hCCCC33;
                8'hAA: color_data <= 24'hCCCC66;
                8'hAB: color_data <= 24'hCCCC99;
                8'hAC: color_data <= 24'hCCCCCC;
                8'hAD: color_data <= 24'hCCCCFF;
                8'hAE: color_data <= 24'hCCFF00;
                8'hAF: color_data <= 24'hCCFF33;
                8'hB0: color_data <= 24'hCCFF66;
                8'hB1: color_data <= 24'hCCFF99;
                8'hB2: color_data <= 24'hCCFFCC;
                8'hB3: color_data <= 24'hCCFFFF;
                8'hB4: color_data <= 24'hFF0000;
                8'hB5: color_data <= 24'hFF0033;
                8'hB6: color_data <= 24'hFF0066;
                8'hB7: color_data <= 24'hFF0099;
                8'hB8: color_data <= 24'hFF00CC;
                8'hB9: color_data <= 24'hFF00FF;
                8'hBA: color_data <= 24'hFF3300;
                8'hBB: color_data <= 24'hFF3333;
                8'hBC: color_data <= 24'hFF3366;
                8'hBD: color_data <= 24'hFF3399;
                8'hBE: color_data <= 24'hFF33CC;
                8'hBF: color_data <= 24'hFF33FF;
                8'hC0: color_data <= 24'hFF6600;
                8'hC1: color_data <= 24'hFF6633;
                8'hC2: color_data <= 24'hFF6666;
                8'hC3: color_data <= 24'hFF6699;
                8'hC4: color_data <= 24'hFF66CC;
                8'hC5: color_data <= 24'hFF66FF;
                8'hC6: color_data <= 24'hFF9900;
                8'hC7: color_data <= 24'hFF9933;
                8'hC8: color_data <= 24'hFF9966;
                8'hC9: color_data <= 24'hFF9999;
                8'hCA: color_data <= 24'hFF99CC;
                8'hCB: color_data <= 24'hFF99FF;
                8'hCC: color_data <= 24'hFFCC00;
                8'hCD: color_data <= 24'hFFCC33;
                8'hCE: color_data <= 24'hFFCC66;
                8'hCF: color_data <= 24'hFFCC99;
                8'hD0: color_data <= 24'hFFCCCC;
                8'hD1: color_data <= 24'hFFCCFF;
                8'hD2: color_data <= 24'hFFFF00;
                8'hD3: color_data <= 24'hFFFF33;
                8'hD4: color_data <= 24'hFFFF66;
                8'hD5: color_data <= 24'hFFFF99;
                8'hD6: color_data <= 24'hFFFFCC;
                8'hD7: color_data <= 24'hFFFFFF;
                8'hD8: color_data <= 24'h000000;
                8'hD9: color_data <= 24'h2F5B89;
                8'hDA: color_data <= 24'h5EB612;
                8'hDB: color_data <= 24'h8D119B;
                8'hDC: color_data <= 24'hBC6C24;
                8'hDD: color_data <= 24'hEBC7AD;
                8'hDE: color_data <= 24'h1A2236;
                8'hDF: color_data <= 24'h497DBF;
                8'hE0: color_data <= 24'h78D848;
                8'hE1: color_data <= 24'hA733D1;
                8'hE2: color_data <= 24'hD68E5A;
                8'hE3: color_data <= 24'h05E9E3;
                8'hE4: color_data <= 24'h34446C;
                8'hE5: color_data <= 24'h639FF5;
                8'hE6: color_data <= 24'h92FA7E;
                8'hE7: color_data <= 24'hC15507;
                8'hE8: color_data <= 24'hF0B090;
                8'hE9: color_data <= 24'h1F0B19;
                8'hEA: color_data <= 24'h4E66A2;
                8'hEB: color_data <= 24'h7DC12B;
                8'hEC: color_data <= 24'hAC1CB4;
                8'hED: color_data <= 24'hDB773D;
                8'hEE: color_data <= 24'h0AD2C6;
                8'hEF: color_data <= 24'h392D4F;
                8'hF0: color_data <= 24'h6888D8;
                8'hF1: color_data <= 24'h97E361;
                8'hF2: color_data <= 24'hC63EEA;
                8'hF3: color_data <= 24'hF59973;
                8'hF4: color_data <= 24'h24F4FC;
                8'hF5: color_data <= 24'h534F85;
                8'hF6: color_data <= 24'h82AA0E;
                8'hF7: color_data <= 24'hB10597;
                8'hF8: color_data <= 24'hE06020;
                8'hF9: color_data <= 24'h0FBBA9;
                8'hFA: color_data <= 24'h3E1632;
                8'hFB: color_data <= 24'h6D71BB;
                8'hFC: color_data <= 24'h9CCC44;
                8'hFD: color_data <= 24'hCB27CD;
                8'hFE: color_data <= 24'hFA8256;
                8'hFF: color_data <= 24'h29DDDF;
                default: color_data <= 24'h000000;
            endcase
        end
    end
endmodule

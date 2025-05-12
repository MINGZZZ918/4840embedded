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
    soc_system_rom_sprites sprite_images0 (
        .address   (sprite_rom_addr),
        .chipselect(1'b1), 
        .clk(clk), 
        .clken(1'b1),
        .debugaccess(1'b0), 
        .freeze(1'b0),
        .reset     (1'b0), 
        .reset_req(1'b0),
        .write     (1'b0), 
        .writedata(32'b0),
        .readdata  (rom_data0)
    );
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
    logic [23:0] sprite_data = color_data;

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

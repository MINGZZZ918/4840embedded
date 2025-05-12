/*
 * Avalon memory-mapped peripheral for VGA Ball Game
 */

module vga_ball#(
    parameter MAX_OBJECTS = 100,    // sprites数量（adress传递的值最后给obj_sprite，这个才是精灵种类，最多64种）
    parameter SPRITE_WIDTH = 32,   // 所有精灵标准宽度
    parameter SPRITE_HEIGHT = 32  // 所有精灵标准高度
) (
    input  logic        clk,
    input  logic        reset,
    input  logic [31:0] writedata,  // 改为32位宽度
    input  logic        write,
    input  logic        chipselect,
    input  logic [6:0]  address,    // 由于一次传32位，地址空间可以减小
    output logic [7:0]  VGA_R, VGA_G, VGA_B,
    output logic        VGA_CLK, VGA_HS, VGA_VS,
    output logic        VGA_BLANK_n,
    output logic        VGA_SYNC_n
);

    // 常量定义
    logic [10:0]    hcount;
    logic [9:0]     vcount;

    // Background color
    logic [7:0]     background_r, background_g, background_b;
    
    // 游戏对象数组，每个对象包含位置和精灵信息（不懂）
    logic [11:0]    obj_x[MAX_OBJECTS]; // 12位x坐标
    logic [11:0]    obj_y[MAX_OBJECTS]; // 12位y坐标
    logic [5:0]     obj_sprite[MAX_OBJECTS]; // 6位精灵索引，所以最多是64个精灵
    logic           obj_active[MAX_OBJECTS]; // 活动状态位
    
    
    // 精灵渲染相关
    localparam int SPRITE_SIZE  = SPRITE_WIDTH * SPRITE_HEIGHT; // 32 * 32 = 1024
    // full sprite_address spans all 8 ROMs
    logic [14:0] sprite_address;
    // split off the top 3 bits to pick which ROM out of 8
    wire [2:0]  sprite_rom_sel  = sprite_address[14:12];
    // low 12 bits index into whichever ROM
    wire [11:0] sprite_rom_addr = sprite_address[11:0];

    logic [7:0] color_address;
    logic [7:0] rom_data;
    logic [7:0] rom_r, rom_g, rom_b;
    logic [23:0] sprite_data;
    logic [23:0] color_data;  // 来自 color_palette.mif 的 RGB
    logic        transparent_part; // 精灵中的透明背景可以透出下面的精灵


    // ROM IP module
    //rom_sprites
    soc_system_rom_sprites sprite_images (
        .address      (sprite_address),   // ROM 索引地址
        .chipselect   (1'b1),             // 始终使能
        .clk          (clk),              // 时钟
        .clken        (1'b1),             // 时钟使能
        .debugaccess  (1'b0),
        .freeze       (1'b0),
        .reset        (1'b0),
        .reset_req    (1'b0),
        .write        (1'b0),
        .writedata    (32'b0),
        .readdata     (rom_data)          // 输出：ROM中读出的颜色索引
    );

    //color palette
    color_palette palette_inst (
        .clk        (clk),
        .clken      (1'b1),
        .address    (color_address),
        .color_data (color_data)
    );
    assign color_address = rom_data;
    assign sprite_data = color_data;
    assign {rom_r, rom_g, rom_b} = sprite_data;

    // Instantiate VGA counter module
    vga_counters counters(.clk50(clk), .*);

    // Register update logic
    always_ff @(posedge clk) begin //initialize
        if (reset) begin
            // 初始化背景色
            background_r <= 8'h00;
            background_g <= 8'h80;
            background_b <= 8'h00;  // 深蓝色背景
            
            // 初始化所有对象
            for (int i = 0; i < MAX_OBJECTS; i++) begin
                obj_x[i] <= 12'd0;
                obj_y[i] <= 12'd0;
                obj_sprite[i] <= 6'd0;
                obj_active[i] <= 1'b0;
            end

        end 

        else if (chipselect && write) begin
            case (address)
                // 设置背景色 - 使用一个32位写入
                5'd0: {background_r, background_g, background_b} <= writedata[23:0];
                //如果想在sw设置敌人和子弹数量可以在bg这里传，剩下8bit
                // 对象数据更新 - 地址1到MAX_OBJECTS对应各个对象
                default: begin
                    if (address >= 7'd1 && address <= 7'd1 + MAX_OBJECTS - 1) begin //最先打印的是 bg，然后先传 ship，再传敌人，再传子弹
                        int obj_idx;
                        obj_idx = address - 7'd1;
                        // 解析32位数据
                        obj_x[obj_idx] <= writedata[31:20];     // 高12位是x坐标
                        obj_y[obj_idx] <= writedata[19:8];      // 接下来12位是y坐标
                        obj_sprite[obj_idx] <= writedata[7:2];  // 接下来6位是精灵索引(64 种精灵图案)
                        obj_active[obj_idx] <= writedata[1];    // 接下来1位是活动状态
                        // 最低位保留，不使用
                    end
                end
            endcase
        end
    end

    // 渲染逻辑 - 确定当前像素属于哪个对象
    logic [6:0] active_obj_idx;
    logic obj_visible;
    logic found; //用来判断有没有找到非透明的像素，没有就继续，直到最后
    logic [4:0] rel_x, rel_y;
    logic [23:0] pix; //保留当前层的rgb数据
    logic [23:0] pix_candidate; //
    always_comb begin
        found = 1'b0;
        obj_visible = 1'b0;
        active_obj_idx = 7'd0;
        rel_x = 4'd0;
        rel_y = 4'd0;
        pix             = {background_r,background_g,background_b};
        pix_candidate   = {background_r,background_g,background_b};
        sprite_address = 14'd0;
        
        // 从高优先级到低优先级检查对象（最后绘制的对象优先级最高）
        for (int i = MAX_OBJECTS - 1; i >= 0; i--) begin
            if (!found &&
                obj_active[i] && 
                hcount[10:1] >= obj_x[i][9:0] && 
                hcount[10:1] < obj_x[i][9:0] + SPRITE_WIDTH &&
                vcount[9:0] >= obj_y[i][9:0] && 
                vcount[9:0] < obj_y[i][9:0] + SPRITE_HEIGHT) begin
                
                active_obj_idx = i[6:0];
                rel_x = hcount[10:1] - obj_x[i][9:0];
                rel_y = vcount[9:0] - obj_y[i][9:0];
                sprite_address = obj_sprite[active_obj_idx] * SPRITE_SIZE 
                                + rel_y * SPRITE_WIDTH 
                                + rel_x;
                pix_candidate = sprite_data;

                if (pix_candidate != 24'h000000) begin
                    pix   = pix_candidate;
                    found = 1'b1;       // 找到第一层非透明，后面就不用再看了
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
                8'd0: color_data <= 24'h000000;
                8'd1: color_data <= 24'h000033;
                8'd2: color_data <= 24'h000066;
                8'd3: color_data <= 24'h000099;
                8'd4: color_data <= 24'h0000CC;
                8'd5: color_data <= 24'h0000FF;
                8'd6: color_data <= 24'h003300;
                8'd7: color_data <= 24'h003333;
                8'd8: color_data <= 24'h003366;
                8'd9: color_data <= 24'h003399;
                8'd10: color_data <= 24'h0033CC;
                8'd11: color_data <= 24'h0033FF;
                8'd12: color_data <= 24'h006600;
                8'd13: color_data <= 24'h006633;
                8'd14: color_data <= 24'h006666;
                8'd15: color_data <= 24'h006699;
                8'd16: color_data <= 24'h0066CC;
                8'd17: color_data <= 24'h0066FF;
                8'd18: color_data <= 24'h009900;
                8'd19: color_data <= 24'h009933;
                8'd20: color_data <= 24'h009966;
                8'd21: color_data <= 24'h009999;
                8'd22: color_data <= 24'h0099CC;
                8'd23: color_data <= 24'h0099FF;
                8'd24: color_data <= 24'h00CC00;
                8'd25: color_data <= 24'h00CC33;
                8'd26: color_data <= 24'h00CC66;
                8'd27: color_data <= 24'h00CC99;
                8'd28: color_data <= 24'h00CCCC;
                8'd29: color_data <= 24'h00CCFF;
                8'd30: color_data <= 24'h00FF00;
                8'd31: color_data <= 24'h00FF33;
                8'd32: color_data <= 24'h00FF66;
                8'd33: color_data <= 24'h00FF99;
                8'd34: color_data <= 24'h00FFCC;
                8'd35: color_data <= 24'h00FFFF;
                8'd36: color_data <= 24'h330000;
                8'd37: color_data <= 24'h330033;
                8'd38: color_data <= 24'h330066;
                8'd39: color_data <= 24'h330099;
                8'd40: color_data <= 24'h3300CC;
                8'd41: color_data <= 24'h3300FF;
                8'd42: color_data <= 24'h333300;
                8'd43: color_data <= 24'h333333;
                8'd44: color_data <= 24'h333366;
                8'd45: color_data <= 24'h333399;
                8'd46: color_data <= 24'h3333CC;
                8'd47: color_data <= 24'h3333FF;
                8'd48: color_data <= 24'h336600;
                8'd49: color_data <= 24'h336633;
                8'd50: color_data <= 24'h336666;
                8'd51: color_data <= 24'h336699;
                8'd52: color_data <= 24'h3366CC;
                8'd53: color_data <= 24'h3366FF;
                8'd54: color_data <= 24'h339900;
                8'd55: color_data <= 24'h339933;
                8'd56: color_data <= 24'h339966;
                8'd57: color_data <= 24'h339999;
                8'd58: color_data <= 24'h3399CC;
                8'd59: color_data <= 24'h3399FF;
                8'd60: color_data <= 24'h33CC00;
                8'd61: color_data <= 24'h33CC33;
                8'd62: color_data <= 24'h33CC66;
                8'd63: color_data <= 24'h33CC99;
                8'd64: color_data <= 24'h33CCCC;
                8'd65: color_data <= 24'h33CCFF;
                8'd66: color_data <= 24'h33FF00;
                8'd67: color_data <= 24'h33FF33;
                8'd68: color_data <= 24'h33FF66;
                8'd69: color_data <= 24'h33FF99;
                8'd70: color_data <= 24'h33FFCC;
                8'd71: color_data <= 24'h33FFFF;
                8'd72: color_data <= 24'h660000;
                8'd73: color_data <= 24'h660033;
                8'd74: color_data <= 24'h660066;
                8'd75: color_data <= 24'h660099;
                8'd76: color_data <= 24'h6600CC;
                8'd77: color_data <= 24'h6600FF;
                8'd78: color_data <= 24'h663300;
                8'd79: color_data <= 24'h663333;
                8'd80: color_data <= 24'h663366;
                8'd81: color_data <= 24'h663399;
                8'd82: color_data <= 24'h6633CC;
                8'd83: color_data <= 24'h6633FF;
                8'd84: color_data <= 24'h666600;
                8'd85: color_data <= 24'h666633;
                8'd86: color_data <= 24'h666666;
                8'd87: color_data <= 24'h666699;
                8'd88: color_data <= 24'h6666CC;
                8'd89: color_data <= 24'h6666FF;
                8'd90: color_data <= 24'h669900;
                8'd91: color_data <= 24'h669933;
                8'd92: color_data <= 24'h669966;
                8'd93: color_data <= 24'h669999;
                8'd94: color_data <= 24'h6699CC;
                8'd95: color_data <= 24'h6699FF;
                8'd96: color_data <= 24'h66CC00;
                8'd97: color_data <= 24'h66CC33;
                8'd98: color_data <= 24'h66CC66;
                8'd99: color_data <= 24'h66CC99;
                8'd100: color_data <= 24'h66CCCC;
                8'd101: color_data <= 24'h66CCFF;
                8'd102: color_data <= 24'h66FF00;
                8'd103: color_data <= 24'h66FF33;
                8'd104: color_data <= 24'h66FF66;
                8'd105: color_data <= 24'h66FF99;
                8'd106: color_data <= 24'h66FFCC;
                8'd107: color_data <= 24'h66FFFF;
                8'd108: color_data <= 24'h990000;
                8'd109: color_data <= 24'h990033;
                8'd110: color_data <= 24'h990066;
                8'd111: color_data <= 24'h990099;
                8'd112: color_data <= 24'h9900CC;
                8'd113: color_data <= 24'h9900FF;
                8'd114: color_data <= 24'h993300;
                8'd115: color_data <= 24'h993333;
                8'd116: color_data <= 24'h993366;
                8'd117: color_data <= 24'h993399;
                8'd118: color_data <= 24'h9933CC;
                8'd119: color_data <= 24'h9933FF;
                8'd120: color_data <= 24'h996600;
                8'd121: color_data <= 24'h996633;
                8'd122: color_data <= 24'h996666;
                8'd123: color_data <= 24'h996699;
                8'd124: color_data <= 24'h9966CC;
                8'd125: color_data <= 24'h9966FF;
                8'd126: color_data <= 24'h999900;
                8'd127: color_data <= 24'h999933;
                8'd128: color_data <= 24'h999966;
                8'd129: color_data <= 24'h999999;
                8'd130: color_data <= 24'h9999CC;
                8'd131: color_data <= 24'h9999FF;
                8'd132: color_data <= 24'h99CC00;
                8'd133: color_data <= 24'h99CC33;
                8'd134: color_data <= 24'h99CC66;
                8'd135: color_data <= 24'h99CC99;
                8'd136: color_data <= 24'h99CCCC;
                8'd137: color_data <= 24'h99CCFF;
                8'd138: color_data <= 24'h99FF00;
                8'd139: color_data <= 24'h99FF33;
                8'd140: color_data <= 24'h99FF66;
                8'd141: color_data <= 24'h99FF99;
                8'd142: color_data <= 24'h99FFCC;
                8'd143: color_data <= 24'h99FFFF;
                8'd144: color_data <= 24'hCC0000;
                8'd145: color_data <= 24'hCC0033;
                8'd146: color_data <= 24'hCC0066;
                8'd147: color_data <= 24'hCC0099;
                8'd148: color_data <= 24'hCC00CC;
                8'd149: color_data <= 24'hCC00FF;
                8'd150: color_data <= 24'hCC3300;
                8'd151: color_data <= 24'hCC3333;
                8'd152: color_data <= 24'hCC3366;
                8'd153: color_data <= 24'hCC3399;
                8'd154: color_data <= 24'hCC33CC;
                8'd155: color_data <= 24'hCC33FF;
                8'd156: color_data <= 24'hCC6600;
                8'd157: color_data <= 24'hCC6633;
                8'd158: color_data <= 24'hCC6666;
                8'd159: color_data <= 24'hCC6699;
                8'd160: color_data <= 24'hCC66CC;
                8'd161: color_data <= 24'hCC66FF;
                8'd162: color_data <= 24'hCC9900;
                8'd163: color_data <= 24'hCC9933;
                8'd164: color_data <= 24'hCC9966;
                8'd165: color_data <= 24'hCC9999;
                8'd166: color_data <= 24'hCC99CC;
                8'd167: color_data <= 24'hCC99FF;
                8'd168: color_data <= 24'hCCCC00;
                8'd169: color_data <= 24'hCCCC33;
                8'd170: color_data <= 24'hCCCC66;
                8'd171: color_data <= 24'hCCCC99;
                8'd172: color_data <= 24'hCCCCCC;
                8'd173: color_data <= 24'hCCCCFF;
                8'd174: color_data <= 24'hCCFF00;
                8'd175: color_data <= 24'hCCFF33;
                8'd176: color_data <= 24'hCCFF66;
                8'd177: color_data <= 24'hCCFF99;
                8'd178: color_data <= 24'hCCFFCC;
                8'd179: color_data <= 24'hCCFFFF;
                8'd180: color_data <= 24'hFF0000;
                8'd181: color_data <= 24'hFF0033;
                8'd182: color_data <= 24'hFF0066;
                8'd183: color_data <= 24'hFF0099;
                8'd184: color_data <= 24'hFF00CC;
                8'd185: color_data <= 24'hFF00FF;
                8'd186: color_data <= 24'hFF3300;
                8'd187: color_data <= 24'hFF3333;
                8'd188: color_data <= 24'hFF3366;
                8'd189: color_data <= 24'hFF3399;
                8'd190: color_data <= 24'hFF33CC;
                8'd191: color_data <= 24'hFF33FF;
                8'd192: color_data <= 24'hFF6600;
                8'd193: color_data <= 24'hFF6633;
                8'd194: color_data <= 24'hFF6666;
                8'd195: color_data <= 24'hFF6699;
                8'd196: color_data <= 24'hFF66CC;
                8'd197: color_data <= 24'hFF66FF;
                8'd198: color_data <= 24'hFF9900;
                8'd199: color_data <= 24'hFF9933;
                8'd200: color_data <= 24'hFF9966;
                8'd201: color_data <= 24'hFF9999;
                8'd202: color_data <= 24'hFF99CC;
                8'd203: color_data <= 24'hFF99FF;
                8'd204: color_data <= 24'hFFCC00;
                8'd205: color_data <= 24'hFFCC33;
                8'd206: color_data <= 24'hFFCC66;
                8'd207: color_data <= 24'hFFCC99;
                8'd208: color_data <= 24'hFFCCCC;
                8'd209: color_data <= 24'hFFCCFF;
                8'd210: color_data <= 24'hFFFF00;
                8'd211: color_data <= 24'hFFFF33;
                8'd212: color_data <= 24'hFFFF66;
                8'd213: color_data <= 24'hFFFF99;
                8'd214: color_data <= 24'hFFFFCC;
                8'd215: color_data <= 24'hFFFFFF;
                8'd216: color_data <= 24'h000000;
                8'd217: color_data <= 24'h2F5B89;
                8'd218: color_data <= 24'h5EB612;
                8'd219: color_data <= 24'h8D119B;
                8'd220: color_data <= 24'hBC6C24;
                8'd221: color_data <= 24'hEBC7AD;
                8'd222: color_data <= 24'h1A2236;
                8'd223: color_data <= 24'h497DBF;
                8'd224: color_data <= 24'h78D848;
                8'd225: color_data <= 24'hA733D1;
                8'd226: color_data <= 24'hD68E5A;
                8'd227: color_data <= 24'h05E9E3;
                8'd228: color_data <= 24'h34446C;
                8'd229: color_data <= 24'h639FF5;
                8'd230: color_data <= 24'h92FA7E;
                8'd231: color_data <= 24'hC15507;
                8'd232: color_data <= 24'hF0B090;
                8'd233: color_data <= 24'h1F0B19;
                8'd234: color_data <= 24'h4E66A2;
                8'd235: color_data <= 24'h7DC12B;
                8'd236: color_data <= 24'hAC1CB4;
                8'd237: color_data <= 24'hDB773D;
                8'd238: color_data <= 24'h0AD2C6;
                8'd239: color_data <= 24'h392D4F;
                8'd240: color_data <= 24'h6888D8;
                8'd241: color_data <= 24'h97E361;
                8'd242: color_data <= 24'hC63EEA;
                8'd243: color_data <= 24'hF59973;
                8'd244: color_data <= 24'h24F4FC;
                8'd245: color_data <= 24'h534F85;
                8'd246: color_data <= 24'h82AA0E;
                8'd247: color_data <= 24'hB10597;
                8'd248: color_data <= 24'hE06020;
                8'd249: color_data <= 24'h0FBBA9;
                8'd250: color_data <= 24'h3E1632;
                8'd251: color_data <= 24'h6D71BB;
                8'd252: color_data <= 24'h9CCC44;
                8'd253: color_data <= 24'hCB27CD;
                8'd254: color_data <= 24'hFA8256;
                8'd255: color_data <= 24'h29DDDF;
                default: color_data <= 24'h000000;
            endcase
        end
    end
endmodule
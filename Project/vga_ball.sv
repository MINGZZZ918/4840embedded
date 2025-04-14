/*
 * Avalon memory-mapped peripheral for VGA Ball Game
 *
 * Register Map:
 * 
 * Byte Offset    Meaning
 *        0     |  Background Red
 *        1     |  Background Green
 *        2     |  Background Blue
 *        3     |  Ship X position (lower 8 bits)
 *        4     |  Ship X position (upper 3 bits)
 *        5     |  Ship Y position (lower 8 bits)
 *        6     |  Ship Y position (upper 2 bits)
 *        7     |  Bullet 0 X position (lower 8 bits)
 *        ...   |  ...
 *        28    |  Bullets Active Bits (bit 0-4 correspond to bullets 0-4)
 *        29    |  Image X position (lower 8 bits)
 *        30    |  Image X position (upper 3 bits)
 *        31    |  Image Y position (lower 8 bits)
 *        32    |  Image Y position (upper 2 bits)
 *        33    |  Image Display Flag (1 = display, 0 = hide)
 *        34    |  Image Data (RGB pixel data, 64x64x3 bytes)
 */

module vga_ball(
    input  logic        clk,
    input  logic        reset,
    input  logic [7:0]  writedata,
    input  logic        write,
    input  logic        chipselect,
    input  logic [15:0] address,  // 扩展地址位以适应图片数据

    output logic [7:0]  VGA_R, VGA_G, VGA_B,
    output logic        VGA_CLK, VGA_HS, VGA_VS,
                        VGA_BLANK_n,
    output logic        VGA_SYNC_n
);

    // 常量定义
    parameter MAX_BULLETS = 5;   // 最大子弹数
    parameter IMAGE_WIDTH = 64;  // 图片宽度
    parameter IMAGE_HEIGHT = 64; // 图片高度

    logic [10:0]    hcount;
    logic [9:0]     vcount;

    // Background color
    logic [7:0]     background_r, background_g, background_b;
    
    // Spaceship position and properties
    logic [10:0]    ship_x;
    logic [9:0]     ship_y;
    parameter SHIP_WIDTH = 16;   // 调整为像素艺术飞船的宽度
    parameter SHIP_HEIGHT = 16;  // 调整为像素艺术飞船的高度
    
    // 多个子弹的属性
    logic [10:0]    bullet_x[MAX_BULLETS];
    logic [9:0]     bullet_y[MAX_BULLETS];
    logic [MAX_BULLETS-1:0] bullet_active;  // 子弹活动状态位图
    parameter BULLET_SIZE = 4;
    
    // 图片属性
    logic [10:0]    image_x;
    logic [9:0]     image_y;
    logic           image_display;
    logic [7:0]     image_data[IMAGE_HEIGHT][IMAGE_WIDTH][3]; // RGB数据

    // 飞船像素艺术模式 (16x16)
    // 0=黑色(透明), 1=灰白色, 2=红色, 3=蓝色, 4=紫色
    logic [2:0] ship_pattern[0:15][0:15];
    
    // Instantiate VGA counter module
    vga_counters counters(.clk50(clk), .*);

    // Register update logic
    always_ff @(posedge clk) begin
        if (reset) begin
            // Initialize default values
            background_r <= 8'h00;
            background_g <= 8'h00;
            background_b <= 8'h20;  // Dark blue background
            
            // Initial positions
            ship_x <= 11'd200;
            ship_y <= 10'd240;
            
            // 初始化所有子弹
            for (int i = 0; i < MAX_BULLETS; i++) begin
                bullet_x[i] <= 11'd0;
                bullet_y[i] <= 10'd0;
            end
            bullet_active <= '0;  // 所有子弹都不活动
            
            // 初始化图片数据
            image_x <= 11'd100;
            image_y <= 10'd100;
            image_display <= 1'b0;
            for (int i = 0; i < IMAGE_HEIGHT; i++) begin
                for (int j = 0; j < IMAGE_WIDTH; j++) begin
                    image_data[i][j][0] <= 8'd0; // R
                    image_data[i][j][1] <= 8'd0; // G
                    image_data[i][j][2] <= 8'd0; // B
                end
            end

            // 初始化飞船像素艺术模式
            // 0行 (顶部)
            for (int x = 0; x < 16; x++) ship_pattern[0][x] = 0;
            // 1行
            for (int x = 0; x < 16; x++) ship_pattern[1][x] = 0;
            ship_pattern[1][7] = 1; ship_pattern[1][8] = 1;
            // 2行
            for (int x = 0; x < 16; x++) ship_pattern[2][x] = 0;
            ship_pattern[2][6] = 1; ship_pattern[2][7] = 1; 
            ship_pattern[2][8] = 1; ship_pattern[2][9] = 1;
            // 3行
            for (int x = 0; x < 16; x++) ship_pattern[3][x] = 0;
            ship_pattern[3][5] = 1; ship_pattern[3][6] = 1;
            ship_pattern[3][7] = 1; ship_pattern[3][8] = 1;
            ship_pattern[3][9] = 1; ship_pattern[3][10] = 1;
            // 4行
            for (int x = 0; x < 16; x++) ship_pattern[4][x] = 0;
            ship_pattern[4][1] = 2; ship_pattern[4][5] = 1;
            ship_pattern[4][6] = 1; ship_pattern[4][7] = 1;
            ship_pattern[4][8] = 1; ship_pattern[4][9] = 1;
            ship_pattern[4][10] = 1; ship_pattern[4][14] = 2;
            // 5行
            for (int x = 0; x < 16; x++) ship_pattern[5][x] = 0;
            ship_pattern[5][2] = 1; ship_pattern[5][5] = 1;
            ship_pattern[5][6] = 3; ship_pattern[5][7] = 1;
            ship_pattern[5][8] = 1; ship_pattern[5][9] = 3;
            ship_pattern[5][10] = 1; ship_pattern[5][13] = 1;
            // 6行
            for (int x = 0; x < 16; x++) ship_pattern[6][x] = 0;
            ship_pattern[6][2] = 1; ship_pattern[6][3] = 1;
            ship_pattern[6][4] = 1; ship_pattern[6][5] = 1;
            ship_pattern[6][6] = 1; ship_pattern[6][7] = 2;
            ship_pattern[6][8] = 2; ship_pattern[6][9] = 1;
            ship_pattern[6][10] = 1; ship_pattern[6][11] = 1;
            ship_pattern[6][12] = 1; ship_pattern[6][13] = 1;
            // 7行
            for (int x = 0; x < 16; x++) ship_pattern[7][x] = 0;
            ship_pattern[7][1] = 1; ship_pattern[7][2] = 1;
            ship_pattern[7][3] = 1; ship_pattern[7][4] = 1;
            ship_pattern[7][5] = 1; ship_pattern[7][6] = 1;
            ship_pattern[7][7] = 2; ship_pattern[7][8] = 2;
            ship_pattern[7][9] = 1; ship_pattern[7][10] = 1;
            ship_pattern[7][11] = 1; ship_pattern[7][12] = 1;
            ship_pattern[7][13] = 1; ship_pattern[7][14] = 1;
            // 8行
            for (int x = 0; x < 16; x++) ship_pattern[8][x] = 0;
            ship_pattern[8][0] = 2; ship_pattern[8][1] = 1;
            ship_pattern[8][2] = 1; ship_pattern[8][3] = 1;
            ship_pattern[8][4] = 1; ship_pattern[8][5] = 1;
            ship_pattern[8][6] = 1; ship_pattern[8][7] = 2;
            ship_pattern[8][8] = 2; ship_pattern[8][9] = 1;
            ship_pattern[8][10] = 1; ship_pattern[8][11] = 1;
            ship_pattern[8][12] = 1; ship_pattern[8][13] = 1;
            ship_pattern[8][14] = 1; ship_pattern[8][15] = 2;
            // 9行
            for (int x = 0; x < 16; x++) ship_pattern[9][x] = 0;
            ship_pattern[9][1] = 1; ship_pattern[9][2] = 1;
            ship_pattern[9][3] = 1; ship_pattern[9][4] = 1;
            ship_pattern[9][5] = 1; ship_pattern[9][6] = 1;
            ship_pattern[9][7] = 1; ship_pattern[9][8] = 1;
            ship_pattern[9][9] = 1; ship_pattern[9][10] = 1;
            ship_pattern[9][11] = 1; ship_pattern[9][12] = 1;
            ship_pattern[9][13] = 1; ship_pattern[9][14] = 1;
            // 10行
            for (int x = 0; x < 16; x++) ship_pattern[10][x] = 0;
            ship_pattern[10][2] = 1; ship_pattern[10][3] = 1;
            ship_pattern[10][4] = 1; ship_pattern[10][5] = 1;
            ship_pattern[10][6] = 1; ship_pattern[10][7] = 2;
            ship_pattern[10][8] = 2; ship_pattern[10][9] = 1;
            ship_pattern[10][10] = 1; ship_pattern[10][11] = 1;
            ship_pattern[10][12] = 1; ship_pattern[10][13] = 1;
            // 11行
            for (int x = 0; x < 16; x++) ship_pattern[11][x] = 0;
            ship_pattern[11][3] = 2; ship_pattern[11][6] = 1;
            ship_pattern[11][7] = 2; ship_pattern[11][8] = 2;
            ship_pattern[11][9] = 1; ship_pattern[11][12] = 2;
            // 12行
            for (int x = 0; x < 16; x++) ship_pattern[12][x] = 0;
            ship_pattern[12][4] = 2; ship_pattern[12][11] = 2;
            // 13行
            for (int x = 0; x < 16; x++) ship_pattern[13][x] = 0;
            // 14行
            for (int x = 0; x < 16; x++) ship_pattern[14][x] = 0;
            // 15行
            for (int x = 0; x < 16; x++) ship_pattern[15][x] = 0;
            ship_pattern[15][7] = 4;
        end 
        else if (chipselect && write) begin
            case (address)
                16'd0: background_r <= writedata;
                16'd1: background_g <= writedata;
                16'd2: background_b <= writedata;
                
                // Ship position
                16'd3: ship_x[7:0] <= writedata;
                16'd4: ship_x[10:8] <= writedata[2:0];
                16'd5: ship_y[7:0] <= writedata;
                16'd6: ship_y[9:8] <= writedata[1:0];
                
                // Bullets - 每个子弹使用4个寄存器
                16'd28: bullet_active <= writedata[MAX_BULLETS-1:0];
                
                // 图片属性
                16'd29: image_x[7:0] <= writedata;
                16'd30: image_x[10:8] <= writedata[2:0];
                16'd31: image_y[7:0] <= writedata;
                16'd32: image_y[9:8] <= writedata[1:0];
                16'd33: image_display <= writedata[0];
                
                default: begin
                    // 子弹位置寄存器
                    if (address >= 16'd7 && address < 16'd7 + 4*MAX_BULLETS) begin
                        int bullet_idx = (address - 16'd7) / 4;  // 确定是哪个子弹
                        int bullet_reg = (address - 16'd7) % 4;  // 确定是子弹的哪个属性
                        
                        case (bullet_reg)
                            0: bullet_x[bullet_idx][7:0] <= writedata;
                            1: bullet_x[bullet_idx][10:8] <= writedata[2:0];
                            2: bullet_y[bullet_idx][7:0] <= writedata;
                            3: bullet_y[bullet_idx][9:8] <= writedata[1:0];
                        endcase
                    end
                    // 图片数据寄存器
                    else if (address >= 16'd34 && address < 16'd34 + IMAGE_HEIGHT*IMAGE_WIDTH*3) begin
                        int offset = address - 16'd34;
                        int pixel_index = offset / 3;
                        int pixel_y = pixel_index / IMAGE_WIDTH;
                        int pixel_x = pixel_index % IMAGE_WIDTH;
                        int color_channel = offset % 3;
                        
                        // 存储RGB数据
                        image_data[pixel_y][pixel_x][color_channel] <= writedata;
                    end
                end
            endcase
        end
    end

    // 飞船显示逻辑 - 使用像素艺术模式
    logic ship_on;
    logic [2:0] ship_pixel_value;

    always_comb begin
        ship_on = 0;
        ship_pixel_value = 0;
        
        // 检查当前像素是否在飞船范围内
        if (hcount >= ship_x && hcount < ship_x + SHIP_WIDTH &&
            vcount >= ship_y && vcount < ship_y + SHIP_HEIGHT) begin
            
            // 计算当前像素在飞船图案中的相对位置
            logic [3:0] rel_x, rel_y;
            rel_x = hcount - ship_x;
            rel_y = vcount - ship_y;
            
            // 获取这个位置上的像素值
            ship_pixel_value = ship_pattern[rel_y][rel_x];
            
            // 如果像素值不为0，则飞船在此位置显示
            if (ship_pixel_value != 0) begin
                ship_on = 1;
            end
        end
    end

    // 多个子弹的显示逻辑
    logic bullet_on;
    always_comb begin
        bullet_on = 0;
        for (int i = 0; i < MAX_BULLETS; i++) begin
            if (bullet_active[i] && 
                hcount >= bullet_x[i] && hcount < bullet_x[i] + BULLET_SIZE &&
                vcount >= bullet_y[i] && vcount < bullet_y[i] + BULLET_SIZE) begin
                bullet_on = 1;
            end
        end
    end
    
    // 图片显示逻辑
    logic image_on;
    logic [7:0] image_pixel_r, image_pixel_g, image_pixel_b;
    
    always_comb begin
        image_on = 0;
        image_pixel_r = 8'd0;
        image_pixel_g = 8'd0;
        image_pixel_b = 8'd0;
        
        // 检查是否应该显示图片，以及当前像素是否在图片范围内
        if (image_display && 
            hcount >= image_x && hcount < image_x + IMAGE_WIDTH &&
            vcount >= image_y && vcount < image_y + IMAGE_HEIGHT) begin
            
            // 计算在图片中的相对位置
            logic [5:0] img_x, img_y; // 6位足够表示0-63
            img_x = hcount - image_x;
            img_y = vcount - image_y;
            
            // 获取像素颜色
            image_pixel_r = image_data[img_y][img_x][0];
            image_pixel_g = image_data[img_y][img_x][1];
            image_pixel_b = image_data[img_y][img_x][2];
            
            // 如果像素不是全黑，则显示图片
            // 这里允许显示黑色像素，但你可以添加阈值检查来实现透明度
            if (image_pixel_r != 8'd0 || image_pixel_g != 8'd0 || image_pixel_b != 8'd0) begin
                image_on = 1;
            end
        end
    end

    // VGA output logic - 增加图片显示优先级
    always_comb begin
        {VGA_R, VGA_G, VGA_B} = {8'h00, 8'h00, 8'h00}; // 默认黑色
        
        if (VGA_BLANK_n) begin
            // 背景色
            {VGA_R, VGA_G, VGA_B} = {background_r, background_g, background_b};
            
            // 图片显示 (优先级高于背景，低于飞船和子弹)
            if (image_on) begin
                {VGA_R, VGA_G, VGA_B} = {image_pixel_r, image_pixel_g, image_pixel_b};
            end
            
            // 飞船显示 (优先级高于背景和图片)
            if (ship_on) begin
                // 根据船的像素值选择颜色
                case (ship_pixel_value)
                    3'd1: {VGA_R, VGA_G, VGA_B} = {8'hD0, 8'hD0, 8'hD0}; // 灰白色
                    3'd2: {VGA_R, VGA_G, VGA_B} = {8'hFF, 8'h40, 8'h40}; // 红色
                    3'd3: {VGA_R, VGA_G, VGA_B} = {8'h40, 8'h60, 8'hFF}; // 蓝色
                    3'd4: {VGA_R, VGA_G, VGA_B} = {8'hA0, 8'h20, 8'hFF}; // 紫色
                    default: {VGA_R, VGA_G, VGA_B} = {8'hFF, 8'hFF, 8'hFF}; // 默认白色
                endcase
            end
            
            // 子弹显示 (最高优先级)
            if (bullet_on) begin
                {VGA_R, VGA_G, VGA_B} = {8'hFF, 8'hFF, 8'h00};  // 黄色子弹
            end
        end
    end

endmodule



// VGA timing generator module
module vga_counters(
    input logic        clk50, reset,
    output logic [10:0] hcount,  // hcount[10:1] is pixel column
    output logic [9:0]  vcount,  // vcount[9:0] is pixel row
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
/*
 * Avalon memory-mapped peripheral for VGA Ball Game
 */

module vga_ball(
    input  logic        clk,
    input  logic        reset,
    input  logic [7:0]  writedata,
    input  logic        write,
    input  logic        chipselect,
    input  logic [4:0]  address,

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
    parameter SHIP_WIDTH = 16;   // 飞船宽度
    parameter SHIP_HEIGHT = 16;  // 飞船高度
    
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
    // 0=黑色(透明), 1=红色, 2=白色, 3=蓝色
    logic [1:0] ship_pattern[16][16]; // 2位宽，支持4种颜色
    
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

            // 初始化飞船像素艺术模式 - 全部初始化为0
            for (int y = 0; y < 16; y++) begin
                for (int x = 0; x < 16; x++) begin
                    ship_pattern[y][x] <= 2'b00;
                end
            end
            
            // 上部 - 设置为红色(1)和白色(2)
            ship_pattern[2][7] <= 2'b01; ship_pattern[2][8] <= 2'b01;
            ship_pattern[3][6] <= 2'b01; ship_pattern[3][7] <= 2'b01; 
            ship_pattern[3][8] <= 2'b01; ship_pattern[3][9] <= 2'b01;
            
            // 中部 - 主体为红色(1)，窗户为白色(2)，引擎为蓝色(3)
            // 行4
            ship_pattern[4][2] <= 2'b01; ship_pattern[4][3] <= 2'b01; ship_pattern[4][4] <= 2'b01;
            ship_pattern[4][5] <= 2'b01; ship_pattern[4][6] <= 2'b01; ship_pattern[4][7] <= 2'b01;
            ship_pattern[4][8] <= 2'b01; ship_pattern[4][9] <= 2'b01; ship_pattern[4][10] <= 2'b01;
            ship_pattern[4][11] <= 2'b01; ship_pattern[4][12] <= 2'b01;
            
            // 行5
            ship_pattern[5][1] <= 2'b01; ship_pattern[5][2] <= 2'b01; ship_pattern[5][3] <= 2'b01;
            ship_pattern[5][4] <= 2'b01; ship_pattern[5][5] <= 2'b01; ship_pattern[5][6] <= 2'b01;
            ship_pattern[5][7] <= 2'b10; ship_pattern[5][8] <= 2'b01; ship_pattern[5][9] <= 2'b01;
            ship_pattern[5][10] <= 2'b01; ship_pattern[5][11] <= 2'b01; ship_pattern[5][12] <= 2'b01;
            ship_pattern[5][13] <= 2'b01;
            
            // 行6
            ship_pattern[6][0] <= 2'b01; ship_pattern[6][1] <= 2'b01; ship_pattern[6][2] <= 2'b01;
            ship_pattern[6][3] <= 2'b01; ship_pattern[6][4] <= 2'b01; ship_pattern[6][5] <= 2'b01;
            ship_pattern[6][6] <= 2'b01; ship_pattern[6][7] <= 2'b01; ship_pattern[6][8] <= 2'b01;
            ship_pattern[6][9] <= 2'b01; ship_pattern[6][10] <= 2'b01; ship_pattern[6][11] <= 2'b01;
            ship_pattern[6][12] <= 2'b01; ship_pattern[6][13] <= 2'b01; ship_pattern[6][14] <= 2'b11;
            
            // 行7
            ship_pattern[7][0] <= 2'b01; ship_pattern[7][1] <= 2'b01; ship_pattern[7][2] <= 2'b01;
            ship_pattern[7][3] <= 2'b01; ship_pattern[7][4] <= 2'b01; ship_pattern[7][5] <= 2'b01;
            ship_pattern[7][6] <= 2'b01; ship_pattern[7][7] <= 2'b01; ship_pattern[7][8] <= 2'b01;
            ship_pattern[7][9] <= 2'b01; ship_pattern[7][10] <= 2'b01; ship_pattern[7][11] <= 2'b01;
            ship_pattern[7][12] <= 2'b01; ship_pattern[7][13] <= 2'b01; ship_pattern[7][14] <= 2'b11;
            ship_pattern[7][15] <= 2'b11;
            
            // 行8
            ship_pattern[8][0] <= 2'b01; ship_pattern[8][1] <= 2'b01; ship_pattern[8][2] <= 2'b01;
            ship_pattern[8][3] <= 2'b01; ship_pattern[8][4] <= 2'b01; ship_pattern[8][5] <= 2'b01;
            ship_pattern[8][6] <= 2'b01; ship_pattern[8][7] <= 2'b01; ship_pattern[8][8] <= 2'b01;
            ship_pattern[8][9] <= 2'b01; ship_pattern[8][10] <= 2'b01; ship_pattern[8][11] <= 2'b01;
            ship_pattern[8][12] <= 2'b01; ship_pattern[8][13] <= 2'b01; ship_pattern[8][14] <= 2'b11;
            ship_pattern[8][15] <= 2'b11;
            
            // 行9
            ship_pattern[9][0] <= 2'b01; ship_pattern[9][1] <= 2'b01; ship_pattern[9][2] <= 2'b01;
            ship_pattern[9][3] <= 2'b01; ship_pattern[9][4] <= 2'b01; ship_pattern[9][5] <= 2'b01;
            ship_pattern[9][6] <= 2'b01; ship_pattern[9][7] <= 2'b01; ship_pattern[9][8] <= 2'b01;
            ship_pattern[9][9] <= 2'b01; ship_pattern[9][10] <= 2'b01; ship_pattern[9][11] <= 2'b01;
            ship_pattern[9][12] <= 2'b01; ship_pattern[9][13] <= 2'b01; ship_pattern[9][14] <= 2'b11;
            
            // 行10
            ship_pattern[10][1] <= 2'b01; ship_pattern[10][2] <= 2'b01; ship_pattern[10][3] <= 2'b01;
            ship_pattern[10][4] <= 2'b01; ship_pattern[10][5] <= 2'b01; ship_pattern[10][6] <= 2'b01;
            ship_pattern[10][7] <= 2'b10; ship_pattern[10][8] <= 2'b01; ship_pattern[10][9] <= 2'b01;
            ship_pattern[10][10] <= 2'b01; ship_pattern[10][11] <= 2'b01; ship_pattern[10][12] <= 2'b01;
            ship_pattern[10][13] <= 2'b01;
            
            // 行11
            ship_pattern[11][2] <= 2'b01; ship_pattern[11][3] <= 2'b01; ship_pattern[11][4] <= 2'b01;
            ship_pattern[11][5] <= 2'b01; ship_pattern[11][6] <= 2'b01; ship_pattern[11][7] <= 2'b01;
            ship_pattern[11][8] <= 2'b01; ship_pattern[11][9] <= 2'b01; ship_pattern[11][10] <= 2'b01;
            ship_pattern[11][11] <= 2'b01; ship_pattern[11][12] <= 2'b01;
            
            // 下部
            ship_pattern[12][6] <= 2'b01; ship_pattern[12][7] <= 2'b01; ship_pattern[12][8] <= 2'b01;
            ship_pattern[12][9] <= 2'b01;
            ship_pattern[13][7] <= 2'b01; ship_pattern[13][8] <= 2'b01;
        end 
        else if (chipselect && write) begin
            case (address)
                5'd0: background_r <= writedata;
                5'd1: background_g <= writedata;
                5'd2: background_b <= writedata;
                
                // Ship position
                5'd3: ship_x[7:0] <= writedata;
                5'd4: ship_x[10:8] <= writedata[2:0];
                5'd5: ship_y[7:0] <= writedata;
                5'd6: ship_y[9:8] <= writedata[1:0];
                
                // Bullets - 每个子弹使用4个寄存器
                5'd28: bullet_active <= writedata[MAX_BULLETS-1:0];
                
                // 图片属性
                5'd29: image_x[7:0] <= writedata;
                5'd30: image_x[10:8] <= writedata[2:0];
                5'd31: image_y[7:0] <= writedata;
                
                default: begin
                    // 子弹位置寄存器
                    if (address >= 5'd7 && address < 5'd7 + 4*MAX_BULLETS) begin
                        int bullet_idx;
                        int bullet_reg;
                        bullet_idx = (address - 5'd7) / 4;  // 确定是哪个子弹
                        bullet_reg = (address - 5'd7) % 4;  // 确定是子弹的哪个属性
                        
                        case (bullet_reg)
                            0: bullet_x[bullet_idx][7:0] <= writedata;
                            1: bullet_x[bullet_idx][10:8] <= writedata[2:0];
                            2: bullet_y[bullet_idx][7:0] <= writedata;
                            3: bullet_y[bullet_idx][9:8] <= writedata[1:0];
                        endcase
                    end
                end
            endcase
        end
    end

    // 修复重复显示问题 - 确保飞船只在指定位置显示一次
    // 添加额外的检查来确保飞船不会重复显示
    logic ship_on;
    logic [1:0] ship_pixel_value;
    logic [3:0] rel_x, rel_y;
    logic [10:0] actual_hcount;
    logic [9:0] actual_vcount;
    
    // 确保使用正确的像素位置，防止重复
    assign actual_hcount = {1'b0, hcount[10:1]};
    assign actual_vcount = vcount;

    always_comb begin
        ship_on = 0;
        ship_pixel_value = 0;
        rel_x = 0;
        rel_y = 0;
        
        // 只有当像素坐标在飞船区域内时才显示飞船
        // 通过直接比较整个坐标来确保唯一性
        if (actual_hcount >= ship_x[10:0] && 
            actual_hcount < ship_x[10:0] + SHIP_WIDTH &&
            actual_vcount >= ship_y && 
            actual_vcount < ship_y + SHIP_HEIGHT) begin
            
            // 计算当前像素在飞船图案中的相对位置
            rel_x = actual_hcount - ship_x[10:0];
            rel_y = actual_vcount - ship_y;
            
            // 确保相对坐标在有效范围内
            if (rel_x < SHIP_WIDTH && rel_y < SHIP_HEIGHT) begin
                // 获取这个位置上的像素值
                ship_pixel_value = ship_pattern[rel_y][rel_x];
                
                // 如果像素值不为0，则飞船在此位置显示
                if (ship_pixel_value != 0) begin
                    ship_on = 1;
                end
            end
        end
    end

    // 多个子弹的显示逻辑 - 修复以防止重复显示
    logic bullet_on;
    
    always_comb begin
        bullet_on = 0;
        
        for (int i = 0; i < MAX_BULLETS; i++) begin
            if (bullet_active[i] && 
                actual_hcount >= bullet_x[i] && 
                actual_hcount < bullet_x[i] + BULLET_SIZE &&
                actual_vcount >= bullet_y[i] && 
                actual_vcount < bullet_y[i] + BULLET_SIZE &&
                bullet_x[i] < 11'd1280) begin
                bullet_on = 1;
            end
        end
    end
    
    // 图片显示逻辑 - 修复以防止重复显示
    logic image_on;
    logic [7:0] image_pixel_r, image_pixel_g, image_pixel_b;
    logic [5:0] img_x, img_y;
    
    always_comb begin
        image_on = 0;
        image_pixel_r = 8'd0;
        image_pixel_g = 8'd0;
        image_pixel_b = 8'd0;
        img_x = 0;
        img_y = 0;
        
        if (image_display && 
            actual_hcount >= image_x && 
            actual_hcount < image_x + IMAGE_WIDTH &&
            actual_vcount >= image_y && 
            actual_vcount < image_y + IMAGE_HEIGHT) begin
            
            // 计算在图片中的相对位置
            img_x = actual_hcount - image_x;
            img_y = actual_vcount - image_y;
            
            // 确保相对坐标在有效范围内
            if (img_x < IMAGE_WIDTH && img_y < IMAGE_HEIGHT) begin
                // 获取像素颜色
                image_pixel_r = image_data[img_y][img_x][0];
                image_pixel_g = image_data[img_y][img_x][1];
                image_pixel_b = image_data[img_y][img_x][2];
                
                // 如果像素不是全黑，则显示图片
                if (image_pixel_r != 8'd0 || image_pixel_g != 8'd0 || image_pixel_b != 8'd0) begin
                    image_on = 1;
                end
            end
        end
    end

    // VGA output logic
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
                    2'b01: {VGA_R, VGA_G, VGA_B} = {8'hE0, 8'h40, 8'h20}; // 红色
                    2'b10: {VGA_R, VGA_G, VGA_B} = {8'hFF, 8'hFF, 8'hFF}; // 白色
                    2'b11: {VGA_R, VGA_G, VGA_B} = {8'h40, 8'hA0, 8'hFF}; // 蓝色
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
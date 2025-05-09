/*
 * Avalon memory-mapped peripheral for VGA Ball Game
 */

module vga_ball(
    input  logic        clk,
    input  logic        reset,
    input  logic [7:0]  writedata,
    input  logic        write,
    input  logic        chipselect,
    input  logic [5:0]  address,

    output logic [7:0]  VGA_R, VGA_G, VGA_B,
    output logic        VGA_CLK, VGA_HS, VGA_VS,
                        VGA_BLANK_n,
    output logic        VGA_SYNC_n
);

    // 常量定义
    parameter MAX_BULLETS = 5;   // 最大子弹数
    parameter MAX_ENEMY_BULLETS = 6; // 最大敌人子弹数
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
    
    // 玩家子弹的属性
    logic [10:0]    bullet_x[MAX_BULLETS];
    logic [9:0]     bullet_y[MAX_BULLETS];
    logic [MAX_BULLETS-1:0] bullet_active;  // 子弹活动状态位图
    parameter BULLET_SIZE = 4;
    
    // 敌人位置和属性
    logic [10:0]    enemy_x[2];
    logic [9:0]     enemy_y[2];
    logic [1:0]     enemy_active;  // 两个敌人的活动状态
    parameter ENEMY_WIDTH = 16;   // 敌人宽度
    parameter ENEMY_HEIGHT = 16;  // 敌人高度

    // 敌人子弹
    logic [10:0]    enemy_bullet_x[MAX_ENEMY_BULLETS];
    logic [9:0]     enemy_bullet_y[MAX_ENEMY_BULLETS];
    logic [MAX_ENEMY_BULLETS-1:0] enemy_bullet_active;  // 敌人子弹活动状态位图
    parameter ENEMY_BULLET_SIZE = 4;
    
    // 图片属性
    logic [10:0]    image_x;
    logic [9:0]     image_y;
    logic           image_display;
    logic [7:0]     image_data[IMAGE_HEIGHT][IMAGE_WIDTH][3]; // RGB数据

    // 飞船像素艺术模式 (16x16)
    // 0=黑色(透明), 1=红色, 2=白色, 3=蓝色
    logic [1:0] ship_pattern[16][16]; // 2位宽，支持4种颜色
    
    // 敌人像素艺术模式 (16x16)
    // 0=黑色(透明), 1=绿色, 2=白色, 3=红色
    logic [1:0] enemy_pattern[16][16]; // 2位宽，支持4种颜色
    

    //test
    logic [13:0] sprite_address;
    logic [7:0] rom_data;
    logic [7:0] rom_r, rom_g, rom_b;
    logic [23:0] sprite_data;
    logic [7:0] color_address;
    logic [23:0] color_data;  // 来自 color_palette.mif 的 RGB
    logic [7:0] rom_data_d1;        // sprite ROM 输出打一拍
    logic [23:0] color_data_d1;     // color palette 打一拍

    parameter ROM_X = 100;  // 固定显示位置
    parameter ROM_Y = 100;

    logic rom_on;
    logic [3:0] rom_rel_x, rom_rel_y;
    assign rom_on = (actual_hcount >= ROM_X && actual_hcount < ROM_X + 16 &&
                    actual_vcount >= ROM_Y && actual_vcount < ROM_Y + 16);

    assign rom_rel_x = actual_hcount - ROM_X;
    assign rom_rel_y = actual_vcount - ROM_Y;

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


    // Instantiate VGA counter module
    vga_counters counters(.clk50(clk), .*);

    //color palette

    assign color_address = rom_data;

    color_palette palette_inst (
        .clk        (clk),
        .clken      (1'b1),
        .address    (color_address),
        .color_data (color_data)
    );

    assign sprite_data = color_data;
    assign {rom_r, rom_g, rom_b} = sprite_data;


    // Register update logic
    always_ff @(posedge clk) begin
        if (reset) begin
            // Initialize default values
            background_r <= 8'h90;
            background_g <= 8'h00;
            background_b <= 8'h00;  // Dark blue background
            
            // Initial positions
            ship_x <= 11'd200;
            ship_y <= 10'd240;
            
            // 初始化所有玩家子弹
            for (int i = 0; i < MAX_BULLETS; i++) begin
                bullet_x[i] <= 11'd0;
                bullet_y[i] <= 10'd0;
            end
            bullet_active <= '0;  // 所有子弹都不活动
            
            // 初始化敌人
            enemy_x[0] <= 11'd800;
            enemy_x[1] <= 11'd800;
            enemy_y[0] <= 10'd150;
            enemy_y[1] <= 10'd350;
            enemy_active <= 2'b11;  // 两个敌人都活动

            // 初始化敌人子弹
            for (int i = 0; i < MAX_ENEMY_BULLETS; i++) begin
                enemy_bullet_x[i] <= 11'd0;
                enemy_bullet_y[i] <= 10'd0;
            end
            enemy_bullet_active <= '0;  // 所有敌人子弹都不活动
            
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
            
            // 初始化敌人像素艺术模式 - 全部初始化为0
            for (int y = 0; y < 16; y++) begin
                for (int x = 0; x < 16; x++) begin
                    enemy_pattern[y][x] <= 2'b00;
                end
            end
            
            // 绘制敌人图案 - 创建一个不同于飞船的外星人样式
            // 头部 - 绿色
            enemy_pattern[2][6] <= 2'b01; enemy_pattern[2][7] <= 2'b01; 
            enemy_pattern[2][8] <= 2'b01; enemy_pattern[2][9] <= 2'b01;
            // 触角
            enemy_pattern[1][5] <= 2'b01; enemy_pattern[1][10] <= 2'b01;
            enemy_pattern[0][4] <= 2'b01; enemy_pattern[0][11] <= 2'b01;

            // 眼睛 - 白色
            enemy_pattern[3][5] <= 2'b10; enemy_pattern[3][10] <= 2'b10;

            // 身体 - 绿色
            for (int x = 4; x < 12; x++) begin
                enemy_pattern[4][x] <= 2'b01;
                enemy_pattern[5][x] <= 2'b01;
                enemy_pattern[6][x] <= 2'b01;
                enemy_pattern[7][x] <= 2'b01;
                enemy_pattern[8][x] <= 2'b01;
                enemy_pattern[9][x] <= 2'b01;
            end

            // 触手
            for (int y = 10; y < 14; y++) begin
                enemy_pattern[y][4] <= 2'b01;
                enemy_pattern[y][7] <= 2'b01;
                enemy_pattern[y][8] <= 2'b01;
                enemy_pattern[y][11] <= 2'b01;
            end
        end 
        //finish initial
        else if (chipselect && write) begin
            case (address)
                6'd0: background_r <= writedata;
                6'd1: background_g <= writedata;
                6'd2: background_b <= writedata;
                
                // Ship position
                6'd3: ship_x[7:0] <= writedata;
                6'd4: ship_x[10:8] <= writedata[2:0];
                6'd5: ship_y[7:0] <= writedata;
                6'd6: ship_y[9:8] <= writedata[1:0];
                
                // Bullet active status
                6'd27: bullet_active <= writedata[MAX_BULLETS-1:0];
                
                // Enemy positions and status
                6'd28: enemy_x[0][7:0] <= writedata;
                6'd29: enemy_x[0][10:8] <= writedata[2:0];
                6'd30: enemy_y[0][7:0] <= writedata;
                6'd31: enemy_y[0][9:8] <= writedata[1:0];
                6'd32: enemy_x[1][7:0] <= writedata;
                6'd33: enemy_x[1][10:8] <= writedata[2:0];
                6'd34: enemy_y[1][7:0] <= writedata;
                6'd35: enemy_y[1][9:8] <= writedata[1:0];
                6'd36: enemy_active <= writedata[1:0];
                
                // Enemy bullets active status
                6'd61: enemy_bullet_active <= writedata[MAX_ENEMY_BULLETS-1:0];
                
                // 图片属性
                6'd62: image_x[7:0] <= writedata;
                6'd63: image_x[10:8] <= writedata[2:0];
                
                default: begin
                    // 子弹位置寄存器
                    if (address >= 6'd7 && address < 6'd7 + 4*MAX_BULLETS) begin
                        int bullet_idx;
                        int bullet_reg;
                        bullet_idx = (address - 6'd7) / 4;  // 确定是哪个子弹
                        bullet_reg = (address - 6'd7) % 4;  // 确定是子弹的哪个属性
                        
                        case (bullet_reg)
                            0: bullet_x[bullet_idx][7:0] <= writedata;
                            1: bullet_x[bullet_idx][10:8] <= writedata[2:0];
                            2: bullet_y[bullet_idx][7:0] <= writedata;
                            3: bullet_y[bullet_idx][9:8] <= writedata[1:0];
                        endcase
                    end
                    // 敌人子弹位置寄存器
                    else if (address >= 6'd37 && address < 6'd37 + 4*MAX_ENEMY_BULLETS) begin
                        int ebullet_idx;
                        int ebullet_reg;
                        ebullet_idx = (address - 6'd37) / 4;  // 确定是哪个敌人子弹
                        ebullet_reg = (address - 6'd37) % 4;  // 确定是子弹的哪个属性
                        
                        case (ebullet_reg)
                            0: enemy_bullet_x[ebullet_idx][7:0] <= writedata;
                            1: enemy_bullet_x[ebullet_idx][10:8] <= writedata[2:0];
                            2: enemy_bullet_y[ebullet_idx][7:0] <= writedata;
                            3: enemy_bullet_y[ebullet_idx][9:8] <= writedata[1:0];
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
    assign actual_hcount = {1'b0,hcount[10:1]};
    assign actual_vcount = vcount;

    always_comb begin
        // 默认值
        ship_on        = 1'b0;
        rel_x          = 4'd0;
        rel_y          = 4'd0;
        sprite_address = 8'd0;

        if (rom_on) begin
            // ROM 区域优先：用 rom_rel_x/rom_rel_y
            rel_x          = rom_rel_x;
            rel_y          = rom_rel_y;
            sprite_address = rel_y * 16 + rel_x;
        end
        else if (actual_hcount >= ship_x && actual_hcount < ship_x + SHIP_WIDTH &&
                 actual_vcount >= ship_y && actual_vcount < ship_y + SHIP_HEIGHT) begin
            // 飞船区域
            rel_x = actual_hcount - ship_x;
            rel_y = actual_vcount - ship_y;
            if (rel_x < SHIP_WIDTH && rel_y < SHIP_HEIGHT) begin
                sprite_address = rel_y * SHIP_WIDTH + rel_x;
                ship_on        = 1'b1;
            end
        end
    end



    // 敌人显示逻辑
    logic enemy_on;
    logic [1:0] enemy_pixel_value;
    logic [3:0] enemy_rel_x, enemy_rel_y;
    logic [0:0] current_enemy;

    always_comb begin
        enemy_on = 0;
        enemy_pixel_value = 0;
        enemy_rel_x = 0;
        enemy_rel_y = 0;
        current_enemy = 0;
        
        for (int i = 0; i < 2; i++) begin
            if (enemy_active[i] && 
                actual_hcount >= enemy_x[i] && 
                actual_hcount < enemy_x[i] + ENEMY_WIDTH &&
                actual_vcount >= enemy_y[i] && 
                actual_vcount < enemy_y[i] + ENEMY_HEIGHT) begin
                
                enemy_rel_x = actual_hcount - enemy_x[i];
                enemy_rel_y = actual_vcount - enemy_y[i];
                
                if (enemy_rel_x < ENEMY_WIDTH && enemy_rel_y < ENEMY_HEIGHT) begin
                    enemy_pixel_value = enemy_pattern[enemy_rel_y][enemy_rel_x];
                    
                    if (enemy_pixel_value != 0) begin
                        enemy_on = 1;
                        current_enemy = i;
                    end
                end
            end
        end
    end

    // 多个玩家子弹的显示逻辑 - 修复以防止重复显示
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
    
    // 敌人子弹显示逻辑
    logic enemy_bullet_on;
    
    always_comb begin
        enemy_bullet_on = 0;
        
        for (int i = 0; i < MAX_ENEMY_BULLETS; i++) begin
            if (enemy_bullet_active[i] && 
                actual_hcount >= enemy_bullet_x[i] && 
                actual_hcount < enemy_bullet_x[i] + ENEMY_BULLET_SIZE &&
                actual_vcount >= enemy_bullet_y[i] && 
                actual_vcount < enemy_bullet_y[i] + ENEMY_BULLET_SIZE) begin
                enemy_bullet_on = 1;
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
    // VGA 输出，优先级：ROM 图像 > 自定义图片 > 敌人 > 飞船 > 子弹 > 背景
    always_comb begin
        // 默认全黑
        {VGA_R, VGA_G, VGA_B} = {8'h00, 8'h00, 8'h00};

        if (VGA_BLANK_n) begin
            // ROM 区域显示
            if (rom_on) begin
                {VGA_R, VGA_G, VGA_B} = sprite_data;
            end
            // 你原来的 image 显示
            else if (image_on) begin
                {VGA_R, VGA_G, VGA_B} = {image_pixel_r, image_pixel_g, image_pixel_b};
            end
            // 敌人显示
            else if (enemy_on) begin
                {VGA_R, VGA_G, VGA_B} = sprite_data;
            end
            // 飞船显示
            else if (ship_on) begin
                {VGA_R, VGA_G, VGA_B} = sprite_data;
            end
            // 玩家子弹
            else if (bullet_on) begin
                {VGA_R, VGA_G, VGA_B} = sprite_data;
            end
            // 敌人子弹
            else if (enemy_bullet_on) begin
                {VGA_R, VGA_G, VGA_B} = sprite_data;
            end
            // 其余区域显示背景色
            else begin
                {VGA_R, VGA_G, VGA_B} = {background_r, background_g, background_b};
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



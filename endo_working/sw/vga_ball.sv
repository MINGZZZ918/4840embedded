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
    logic [7:0] rom_r, rom_g, rom_b;

    assign {rom_r, rom_g, rom_b} = sprite_data;
    logic [7:0] sprite_address;
    logic [31:0] rom_data;
    parameter ROM_X = 100;  // 固定显示位置
    parameter ROM_Y = 100;

    logic rom_on;
    logic [3:0] rom_rel_x, rom_rel_y;
    assign rom_on = (actual_hcount >= ROM_X && actual_hcount < ROM_X + 16 &&
                    actual_vcount >= ROM_Y && actual_vcount < ROM_Y + 16);

    assign rom_rel_x = actual_hcount - ROM_X;
    assign rom_rel_y = actual_vcount - ROM_Y;

    soc_system_rom_sprites sprite_images (
        .address(sprite_address),
        .clk(clk),
        .readdata(rom_data),
                // 以下是固定赋值
        .byteenable (4'b1111),          // 使能全部字节
        .chipselect (1'b1),             // 始终使能 ROM
        .clken      (1'b1),             // 时钟使能开
        .debugaccess(1'b0),             // 禁止调试访问
        .freeze     (1'b0),             // 无冻结逻辑
        .reset      (1'b0),             // 不复位
        .reset_req  (1'b0),             // 无 reset 请求
        .write      (1'b0),             // 不写入
        .writedata  (32'b0)             // 写数据无效
    );
    logic [23:0] sprite_data;
    assign sprite_data = rom_data[23:0];
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
        ship_on = 0;
        rel_x = 0;
        rel_y = 0;
        sprite_address = 8'd0;  // 默认值，避免 latch

        if (actual_hcount >= ship_x && actual_hcount < ship_x + SHIP_WIDTH &&
            actual_vcount >= ship_y && actual_vcount < ship_y + SHIP_HEIGHT) begin
            
            rel_x = actual_hcount - ship_x;
            rel_y = actual_vcount - ship_y;
            
            if (rel_x < SHIP_WIDTH && rel_y < SHIP_HEIGHT) begin
                sprite_address = rel_y * 16 + rel_x;
                ship_on = (rom_data[23:0] != 24'h000000);  // 判断是否非透明像素
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
    always_comb begin
        {VGA_R, VGA_G, VGA_B} = {8'h00, 8'h00, 8'h00}; // 默认黑色
        
        if (VGA_BLANK_n) begin
            // 背景色
            {VGA_R, VGA_G, VGA_B} = {background_r, background_g, background_b};
            
            // 图片显示 (优先级高于背景，低于其他游戏对象)
            if (image_on) begin
                {VGA_R, VGA_G, VGA_B} = sprite_data;
            end
            
            // 敌人显示
            if (enemy_on) begin
                {VGA_R, VGA_G, VGA_B} = sprite_data;
            end
            
            // 飞船显示 (优先级高于敌人)
            if (ship_on) begin
                {VGA_R, VGA_G, VGA_B} = sprite_data;
            end
            
            // 玩家子弹显示 (优先级高于飞船)
            if (bullet_on) begin
                {VGA_R, VGA_G, VGA_B} = sprite_data;
            end
            
            // 敌人子弹显示 (与玩家子弹同优先级)
            if (enemy_bullet_on) begin
                {VGA_R, VGA_G, VGA_B} = sprite_data;
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

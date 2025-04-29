/*
 * Avalon memory-mapped peripheral for VGA Ball Game with dual frame buffer
 */

module vga_ball(
    input  logic        clk,
    input  logic        reset,
    input  logic [31:0] writedata,  // 改为32位宽度
    input  logic        write,
    input  logic        chipselect,
    input  logic [4:0]  address,    // 由于一次传32位，地址空间可以减小 

    output logic [7:0]  VGA_R, VGA_G, VGA_B,
    output logic        VGA_CLK, VGA_HS, VGA_VS,
                        VGA_BLANK_n,
    output logic        VGA_SYNC_n
);

    // 常量定义
    parameter MAX_OBJECTS = 20;    // 飞船(1) + 敌人(2) + 玩家子弹(5) + 敌人子弹(6) + 预留(6)
    parameter SPRITE_WIDTH = 16;   // 所有精灵标准宽度
    parameter SPRITE_HEIGHT = 16;  // 所有精灵标准高度
    parameter SHIP_SPRITE_INDEX = 0;  // 飞船的精灵索引
    parameter ENEMY_SPRITE_START = 1; // 敌人精灵索引开始
    parameter BULLET_SPRITE_START = 17; // 子弹精灵索引开始
    parameter HACTIVE1 = 10'd 640; // 活动显示宽度
    parameter VACTIVE1 = 10'd 480; // 活动显示高度
    
    // VGA时序相关
    logic [10:0]    hcount;
    logic [9:0]     vcount;

    // Background color
    logic [7:0]     background_r, background_g, background_b;
    
    // 游戏对象数组，每个对象包含位置和精灵信息
    logic [11:0]    obj_x[MAX_OBJECTS]; // 12位x坐标
    logic [11:0]    obj_y[MAX_OBJECTS]; // 12位y坐标
    logic [5:0]     obj_sprite[MAX_OBJECTS]; // 6位精灵索引
    logic           obj_active[MAX_OBJECTS]; // 活动状态位
    
    // 渲染顺序数组 - 存储对象ID，按渲染顺序排列（从后到前）
    logic [4:0]     render_order[MAX_OBJECTS];
    
    // 当前扫描位置的精灵数据
    logic [3:0]     rel_x, rel_y;
    logic [4:0]     current_obj_id;
    logic [5:0]     current_sprite_idx;
    logic           current_pixel_valid;
    logic [23:0]    sprite_data;   // 模拟的精灵RGB数据
    
    // 双缓冲控制信号
    logic           current_buffer; // 当前显示的缓冲区 (0 or 1)
    logic           draw_buffer;    // 当前绘制的缓冲区 (1 or 0，与current_buffer相反)
    logic           vga_frame_start; // 新帧开始信号
    logic           pixel_we;       // 像素写使能
    
    // 双缓冲存储器
    // 缓冲区0
    logic [11:0]    buffer0_obj_id[HACTIVE1][VACTIVE1]; // 对象ID (5位) + 精灵索引 (6位) + 标志位 (1位)
    // 缓冲区1
    logic [11:0]    buffer1_obj_id[HACTIVE1][VACTIVE1]; // 对象ID (5位) + 精灵索引 (6位) + 标志位 (1位)
    
    // 行缓冲更新控制
    logic           row_start;      // 当前行开始绘制的标志
    logic [9:0]     draw_x, draw_y; // 当前正在绘制的像素位置
    
    // VGA计数器模块实例化
    vga_counters counters(.clk50(clk), .*);
    
    // 检测新帧开始和行开始
    always_ff @(posedge clk) begin
        if (reset) begin
            vga_frame_start <= 1'b0;
            row_start <= 1'b0;
        end else begin
            // 当垂直计数器返回到起点，水平计数器也在起点时，表示新帧开始
            vga_frame_start <= (vcount == 0) && (hcount == 0);
            
            // 当水平计数器返回到起点时，表示新行开始
            row_start <= (hcount == 0);
        end
    end

    // 双缓冲区切换逻辑 - 在每帧开始时切换
    always_ff @(posedge clk) begin
        if (reset) begin
            current_buffer <= 1'b0;
            draw_buffer <= 1'b1;
        end else if (vga_frame_start) begin
            // 切换缓冲区
            current_buffer <= ~current_buffer;
            draw_buffer <= ~draw_buffer;
        end
    end

    // 绘制控制逻辑 - 控制当前绘制的像素位置
    always_ff @(posedge clk) begin
        if (reset) begin
            draw_x <= 10'd0;
            draw_y <= 10'd0;
            pixel_we <= 1'b0;
        end else if (vga_frame_start) begin
            // 新帧开始，重置绘制位置到左上角
            draw_x <= 10'd0;
            draw_y <= 10'd0;
            pixel_we <= 1'b1;
        end else if (pixel_we) begin
            // 绘制当前位置的像素
            if (draw_x < HACTIVE1 - 1) begin
                // 移动到同一行的下一列
                draw_x <= draw_x + 10'd1;
            end else begin
                // 移动到下一行的第一列
                draw_x <= 10'd0;
                
                if (draw_y < VACTIVE1 - 1) begin
                    // 移动到下一行
                    draw_y <= draw_y + 10'd1;
                end else begin
                    // 整个帧已经绘制完成，停止绘制直到下一帧开始
                    pixel_we <= 1'b0;
                end
            end
        end
    end
    
    // Register update logic - 处理Avalon总线写入
    always_ff @(posedge clk) begin
        if (reset) begin
            // 初始化背景色
            background_r <= 8'h00;
            background_g <= 8'h00;
            background_b <= 8'h20;  // 深蓝色背景
            
            // 初始化所有对象
            for (int i = 0; i < MAX_OBJECTS; i++) begin
                obj_x[i] <= 12'd0;
                obj_y[i] <= 12'd0;
                obj_sprite[i] <= 6'd0;
                obj_active[i] <= 1'b0;
                
                // 初始化渲染顺序
                render_order[i] <= i[4:0];
            end
            
            // 初始化玩家飞船
            obj_x[0] <= 12'd200;
            obj_y[0] <= 12'd240;
            obj_sprite[0] <= SHIP_SPRITE_INDEX;
            obj_active[0] <= 1'b1;
            
            // 初始化敌人
            obj_x[1] <= 12'd500;
            obj_y[1] <= 12'd150;
            obj_sprite[1] <= ENEMY_SPRITE_START;
            obj_active[1] <= 1'b1;
            
            obj_x[2] <= 12'd500;
            obj_y[2] <= 12'd350;
            obj_sprite[2] <= ENEMY_SPRITE_START;
            obj_active[2] <= 1'b1;
        end 
        else if (chipselect && write) begin
            case (address)
                // 设置背景色 - 使用一个32位写入
                5'd0: {background_r, background_g, background_b} <= writedata[23:0];
                
                // 对象数据更新 - 地址1到MAX_OBJECTS对应各个对象
                default: begin
                    if (address >= 5'd1 && address <= 5'd1 + MAX_OBJECTS - 1) begin
                        int obj_idx = address - 5'd1;
                        // 解析32位数据
                        obj_x[obj_idx] <= writedata[31:20];     // 高12位是x坐标
                        obj_y[obj_idx] <= writedata[19:8];      // 接下来12位是y坐标
                        obj_sprite[obj_idx] <= writedata[7:2];  // 接下来6位是精灵索引
                        obj_active[obj_idx] <= writedata[1];    // 接下来1位是活动状态
                        // 最低位保留，不使用
                    end
                end
            endcase
        end
    end

    // 帧缓冲区绘制逻辑 - 将游戏对象信息写入绘制缓冲区
    always_ff @(posedge clk) begin
        if (pixel_we) begin
            // 默认为没有对象
            logic [11:0] pixel_data;
            pixel_data = 12'h000; // 默认无对象
            
            // 检查所有对象，按渲染顺序（从后到前）
            for (int i = 0; i < MAX_OBJECTS; i++) begin
                int obj_idx = render_order[i];
                
                // 检查对象是否活动且与当前绘制位置相交
                if (obj_active[obj_idx] && 
                    draw_x >= obj_x[obj_idx] && 
                    draw_x < obj_x[obj_idx] + SPRITE_WIDTH &&
                    draw_y >= obj_y[obj_idx] && 
                    draw_y < obj_y[obj_idx] + SPRITE_HEIGHT) begin
                    
                    // 存储对象ID和精灵索引
                    pixel_data = {1'b1, obj_sprite[obj_idx], obj_idx[4:0]};
                    
                    // 找到了此位置要显示的对象（最高优先级），无需检查其他对象
                    break;
                end
            end
            
            // 将像素信息写入绘制缓冲区(draw_buffer)
            if (draw_buffer == 1'b0) begin
                buffer0_obj_id[draw_x][draw_y] <= pixel_data;
            end else begin
                buffer1_obj_id[draw_x][draw_y] <= pixel_data;
            end
        end
    end

    // 从当前显示缓冲区读取像素信息
    always_comb begin
        // 获取当前显示缓冲区的像素信息
        logic [11:0] pixel_info;
        
        if (hcount[10:1] < HACTIVE1 && vcount < VACTIVE1) begin
            if (current_buffer == 1'b0) begin
                pixel_info = buffer0_obj_id[hcount[10:1]][vcount];
            end else begin
                pixel_info = buffer1_obj_id[hcount[10:1]][vcount];
            end
            
            // 解析像素信息
            logic valid_bit = pixel_info[11];
            logic [5:0] sprite_idx = pixel_info[10:5];
            logic [4:0] obj_id = pixel_info[4:0];
            
            if (valid_bit) begin
                current_obj_id = obj_id;
                current_sprite_idx = sprite_idx;
                current_pixel_valid = 1'b1;
                
                // 计算对象内相对坐标
                rel_x = hcount[10:1] - obj_x[current_obj_id][3:0];
                rel_y = vcount - obj_y[current_obj_id][3:0];
            end else begin
                current_obj_id = 5'd0;
                current_sprite_idx = 6'd0;
                current_pixel_valid = 1'b0;
                rel_x = 4'd0;
                rel_y = 4'd0;
            end
        end else begin
            current_obj_id = 5'd0;
            current_sprite_idx = 6'd0;
            current_pixel_valid = 1'b0;
            rel_x = 4'd0;
            rel_y = 4'd0;
        end
    end
    
    // 为了示例，我们可以模拟精灵数据而不使用实际的ROM
    always_comb begin
        // 默认值
        sprite_data = 24'h000000; // 黑色透明
        
        // 只有在相对坐标有效时才设置颜色
        if (current_pixel_valid && rel_x < SPRITE_WIDTH && rel_y < SPRITE_HEIGHT) begin
            // 根据不同的精灵索引返回不同的颜色
            case (current_sprite_idx)
                SHIP_SPRITE_INDEX: begin
                    // 飞船 - 红色
                    sprite_data = 24'hE04020; // RGB格式
                end
                
                // 敌人精灵 (索引1-16)
                default: begin
                    if (current_sprite_idx >= ENEMY_SPRITE_START && 
                        current_sprite_idx < BULLET_SPRITE_START) begin
                        // 敌人 - 绿色
                        sprite_data = 24'h20E020;
                    end
                    else begin
                        // 子弹 - 黄色
                        sprite_data = 24'hFFFF00;
                    end
                end
            endcase
            
            // 可以根据相对坐标添加更复杂的图案
            // 例如，飞船中央添加白色窗户
            if (current_sprite_idx == SHIP_SPRITE_INDEX) begin
                if ((rel_x > 4 && rel_x < 8) && (rel_y > 6 && rel_y < 10)) begin
                    sprite_data = 24'hFFFFFF; // 白色窗户
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
            
            // 如果当前像素属于某个对象且对象是可见的，则显示对象的像素
            if (current_pixel_valid) begin
                // 从sprite_data中获取RGB值
                VGA_R = sprite_data[23:16]; // 高8位是R
                VGA_G = sprite_data[15:8];  // 中8位是G
                VGA_B = sprite_data[7:0];   // 低8位是B
                
                // 如果像素是透明色(全黑)，显示背景
                if (sprite_data == 24'h0) begin
                    {VGA_R, VGA_G, VGA_B} = {background_r, background_g, background_b};
                end
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
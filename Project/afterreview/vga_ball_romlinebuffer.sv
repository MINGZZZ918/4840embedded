/*
 * Avalon memory-mapped peripheral for VGA Ball Game with line buffer
 */

module vga_ball(
    input  logic        clk,
    input  logic        reset,
    input  logic [31:0] writedata,
    input  logic        write,
    input  logic        chipselect,
    input  logic [4:0]  address,

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
    parameter HACTIVE1 = 10'd 640; // 活动显示区域宽度

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
    
    // 当前行缓冲区相关信号
    logic           new_line;      // 新行开始标志
    logic [10:0]    buffer_x;      // 当前正在处理的X坐标
    logic           switch_buffer; // 缓冲区切换信号
    
    // 行缓冲区数据 line buffer data
    
    logic [5:0]     line_obj_id[HACTIVE1]; 
    logic [5:0]     line_sprite_idx[HACTIVE1]; 
    logic           line_valid[HACTIVE1];
    
    // 当前渲染状态
    logic [3:0]     rel_x, rel_y;
    logic [4:0]     current_obj_id;
    logic [5:0]     current_sprite_idx;
    logic           current_pixel_valid;
    
    // 行缓冲相关信号
    logic [5:0]     tile_address_display, tile_address_draw;
    logic [9:0]     pixel_address_display, pixel_address_draw;
    logic [255:0]   tile_data_display, tile_data_draw;
    logic [15:0]    pixel_data_display, pixel_data_draw;
    logic           tile_wren_display, tile_wren_draw;
    logic           pixel_wren_display, pixel_wren_draw;
    logic [255:0]   tile_q_display, tile_q_draw;
    logic [15:0]    pixel_q_display, pixel_q_draw;
    
    // 精灵数据生成
    logic [23:0]    sprite_data;
    
    // VGA计数器模块实例化
    vga_counters counters(.clk50(clk), .*);
    
    // 行缓冲模块实例化
    linebuffer lb_inst(
        .clk(clk),
        .reset(reset),
        .switch(switch_buffer),
        .address_tile_display(tile_address_display),
        .address_pixel_display(pixel_address_display),
        .address_tile_draw(tile_address_draw),
        .address_pixel_draw(pixel_address_draw),
        .data_tile_display(tile_data_display),
        .data_pixel_display(pixel_data_display),
        .data_tile_draw(tile_data_draw),
        .data_pixel_draw(pixel_data_draw),
        .wren_tile_display(tile_wren_display),
        .wren_pixel_display(pixel_wren_display),
        .wren_tile_draw(tile_wren_draw),
        .wren_pixel_draw(pixel_wren_draw),
        .q_tile_display(tile_q_display),
        .q_pixel_display(pixel_q_display),
        .q_tile_draw(tile_q_draw),
        .q_pixel_draw(pixel_q_draw)
    );
    
    // 检测新行开始和帧结束(用于缓冲区切换)
    always_ff @(posedge clk) begin
        if (reset) begin
            new_line <= 1'b0;
            switch_buffer <= 1'b0;
        end else begin
            // 当水平计数器返回到起点时，表示新行开始
            new_line <= (hcount == 0); 
            
            // 在帧结束时切换缓冲区（垂直消隐期间）
            switch_buffer <= (vcount == 0) && (hcount == 0);
        end
    end

    // Register update logic - 处理从Avalon总线来的写入
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
            
            buffer_x <= 11'd0;
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
                    end
                end
            endcase
        end
    end

    // 行缓冲区填充逻辑 - 在绘制缓冲区中准备下一帧
    always_ff @(posedge clk) begin
        if (reset) begin
            buffer_x <= 11'd0;
            pixel_wren_draw <= 1'b0;
            tile_wren_draw <= 1'b0;
        end 
        else if (new_line) begin
            // 新行开始，重置缓冲区扫描位置
            buffer_x <= 11'd0;
            pixel_wren_draw <= 1'b0;
        end
        else if (buffer_x < HACTIVE1) begin
            // 为当前像素位置找到最上层的对象
            logic pixel_found;
            logic [4:0] found_obj_id;
            logic [5:0] found_sprite_idx;
            
            pixel_found = 1'b0;
            found_obj_id = 5'd0;
            found_sprite_idx = 6'd0;
            
            // 检查所有对象，按渲染顺序（从后到前）
            for (int i = 0; i < MAX_OBJECTS; i++) begin
                int obj_idx = render_order[i];
                
                // 检查对象是否活动且与当前扫描行相交
                if (obj_active[obj_idx] && 
                    buffer_x >= obj_x[obj_idx] && 
                    buffer_x < obj_x[obj_idx] + SPRITE_WIDTH &&
                    vcount >= obj_y[obj_idx] && 
                    vcount < obj_y[obj_idx] + SPRITE_HEIGHT) begin
                    
                    pixel_found = 1'b1;
                    found_obj_id = obj_idx;
                    found_sprite_idx = obj_sprite[obj_idx];
                    break;  // 找到了最上层对象，停止搜索
                end
            end
            
            // 将像素信息写入绘制缓冲区
            pixel_address_draw <= buffer_x;
            
            // 如果找到对象，则存储其ID和精灵索引
            if (pixel_found) begin
                // 将对象信息编码为16位数据存储在像素缓冲区中
                // 格式: [5:0] sprite_idx, [10:6] obj_id, [15:11] 未使用/标志位
                pixel_data_draw <= {5'b00000, found_obj_id, found_sprite_idx};
            end else begin
                // 使用特殊值表示背景像素
                pixel_data_draw <= 16'h0000;
            }
            
            pixel_wren_draw <= 1'b1;  // 启用写入
            buffer_x <= buffer_x + 11'd1;  // 移动到下一列
        end else begin
            // 行扫描完成，停止写入
            pixel_wren_draw <= 1'b0;
        end
    end

    // 显示逻辑 - 从显示缓冲区读取数据并渲染到VGA
    always_comb begin
        // 设置显示缓冲区读取地址
        pixel_address_display = hcount[10:1] < HACTIVE1 ? hcount[10:1] : 10'd0;
        pixel_wren_display = 1'b0;  // 显示时不写入
        pixel_data_display = 16'd0;
        
        // 瓦片缓冲区暂时不使用
        tile_address_display = 6'd0;
        tile_address_draw = 6'd0;
        tile_wren_display = 1'b0;
        tile_wren_draw = 1'b0;
        tile_data_display = 256'd0;
        tile_data_draw = 256'd0;
        
        // 从缓冲区读取数据
        logic [15:0] pixel_info = pixel_q_display;
        logic [4:0] obj_id = pixel_info[10:6];
        logic [5:0] sprite_idx = pixel_info[5:0];
        logic is_background = (pixel_info == 16'h0000);
        
        // 计算精灵内相对坐标
        if (!is_background && hcount[10:1] < HACTIVE1) begin
            rel_x = hcount[10:1] - obj_x[obj_id][11:0];
            rel_y = vcount - obj_y[obj_id][11:0];
            current_obj_id = obj_id;
            current_sprite_idx = sprite_idx;
            current_pixel_valid = 1'b1;
        end else begin
            rel_x = 4'd0;
            rel_y = 4'd0;
            current_obj_id = 5'd0;
            current_sprite_idx = 6'd0;
            current_pixel_valid = 1'b0;
        end
    end
    
    // 精灵数据生成 - 基于精灵索引和相对坐标生成颜色
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
                    
                    // 飞船中央添加白色窗户
                    if ((rel_x > 4 && rel_x < 8) && (rel_y > 6 && rel_y < 10)) begin
                        sprite_data = 24'hFFFFFF; // 白色窗户
                    end
                end
                
                // 敌人精灵
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
        end
    end

    // VGA输出逻辑
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
/*
 * Avalon memory-mapped peripheral for VGA Ball Game
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
    parameter MAX_OBJECTS = 20;    // 飞船(1) + 敌人(2) + 玩家子弹(5) + 敌人子弹(6) + 预留(6)，可以增加改成敌人
    parameter SPRITE_WIDTH = 16;   // 所有精灵标准宽度
    parameter SPRITE_HEIGHT = 16;  // 所有精灵标准高度
    parameter SHIP_SPRITE_INDEX = 0;  // 飞船的精灵索引
    parameter ENEMY_SPRITE_START = 1; // 敌人精灵索引开始，预设十六个敌人
    parameter BULLET_SPRITE_START = 17; // 子弹精灵索引开始 ，可以增加 MAX_OBJECTS来增加 bullet 数量
    logic [10:0]    hcount;
    logic [9:0]     vcount;

    // Background color
    logic [7:0]     background_r, background_g, background_b;
    
    // 游戏对象数组，每个对象包含位置和精灵信息
    logic [11:0]    obj_x[MAX_OBJECTS]; // 12位x坐标
    logic [11:0]    obj_y[MAX_OBJECTS]; // 12位y坐标
    logic [5:0]     obj_sprite[MAX_OBJECTS]; // 6位精灵索引，所以最多是64个精灵（bullet+ship+enemy）
    logic           obj_active[MAX_OBJECTS]; // 活动状态位
    
    // 精灵渲染相关
    logic [23:0]    sprite_data;   // 模拟的精灵RGB数据

    // Instantiate VGA counter module
    vga_counters counters(.clk50(clk), .*);

    // Register update logic
    always_ff @(posedge clk) begin //initialize
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
            end
            
            // 初始化玩家飞船
            obj_x[0] <= 12'd200;
            obj_y[0] <= 12'd240;
            obj_sprite[0] <= SHIP_SPRITE_INDEX;
            obj_active[0] <= 1'b1;
            
            // 初始化敌人
            obj_x[1] <= 12'd800;
            obj_y[1] <= 12'd150;
            obj_sprite[1] <= ENEMY_SPRITE_START;
            obj_active[1] <= 1'b1;
            
            obj_x[2] <= 12'd800;
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
                    if (address >= 5'd1 && address <= 5'd1 + MAX_OBJECTS - 1) begin //最先打印的是 bg，然后先传 ship，再传敌人，再传子弹
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

// // 精灵ROM实例化
//     sprite_rom sprite_rom_inst(
//         .clk(clk),
//         .addr(sprite_addr),
//         .data(sprite_data)
//     );

    // 渲染逻辑 - 确定当前像素属于哪个对象
    logic [4:0] active_obj_idx;
    logic obj_visible;
    logic [3:0] rel_x, rel_y;
    
    always_comb begin
        obj_visible = 1'b0;
        active_obj_idx = 5'd0;
        rel_x = 4'd0;
        rel_y = 4'd0;
        
        // 从高优先级到低优先级检查对象（最后绘制的对象优先级最高）
        for (int i = MAX_OBJECTS - 1; i >= 0; i--) begin
            if (obj_active[i] && 
                hcount[10:1] >= obj_x[i] && 
                hcount[10:1] < obj_x[i] + SPRITE_WIDTH &&
                vcount >= obj_y[i] && 
                vcount < obj_y[i] + SPRITE_HEIGHT) begin
                
                active_obj_idx = i[4:0];
                rel_x = hcount[10:1] - obj_x[i][11:0];
                rel_y = vcount - obj_y[i][11:0];
                obj_visible = 1'b1;
                break;  // 找到显示对象，退出循环
            end
        end
    end
    
    // 为了示例，我们可以模拟精灵数据而不使用实际的ROM，example

    always_comb begin
        // 默认值
        sprite_data = 24'h000000; // 黑色透明
        
        // 只有在相对坐标有效时才设置颜色
        if (obj_visible && rel_x < SPRITE_WIDTH && rel_y < SPRITE_HEIGHT) begin
            // 根据不同的精灵索引返回不同的颜色
            case (obj_sprite[active_obj_idx])
                SHIP_SPRITE_INDEX: begin
                    // 飞船 - 红色
                    sprite_data = 24'hE04020; // RGB格式
                end
                
                // 敌人精灵 (索引1-16)
                default: begin
                    if (obj_sprite[active_obj_idx] >= ENEMY_SPRITE_START && 
                        obj_sprite[active_obj_idx] < BULLET_SPRITE_START) begin
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
            if (obj_sprite[active_obj_idx] == SHIP_SPRITE_INDEX) begin
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
            if (obj_visible) begin
                // 从sprite_data中获取RGB值
                VGA_R = sprite_data[23:16]; // 高8位是R，sprite_data就是 readdate
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
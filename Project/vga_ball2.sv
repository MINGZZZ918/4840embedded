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
 *        8     |  Bullet 0 X position (upper 3 bits)
 *        9     |  Bullet 0 Y position (lower 8 bits)
 *        10    |  Bullet 0 Y position (upper 2 bits)
 *        11    |  Bullet 1 X position (lower 8 bits)
 *        ...   |  ...
 *        27    |  Bullet 4 Y position (upper 2 bits) 
 *        28    |  Bullets Active Bits (bit 0-4 correspond to bullets 0-4)
 */

module vga_ball(
    input  logic        clk,
    input  logic        reset,
    input  logic [7:0]  writedata,
    input  logic        write,
    input  logic        chipselect,
    input  logic [4:0]  address,  // 扩展地址位以适应更多寄存器

    output logic [7:0]  VGA_R, VGA_G, VGA_B,
    output logic        VGA_CLK, VGA_HS, VGA_VS,
                        VGA_BLANK_n,
    output logic        VGA_SYNC_n
);

    // 常量定义
    parameter MAX_BULLETS = 5;  // 最大子弹数

    logic [10:0]    hcount;
    logic [9:0]     vcount;

    // Background color
    logic [7:0]     background_r, background_g, background_b;
    
    // Spaceship position and properties
    logic [10:0]    ship_x;
    logic [9:0]     ship_y;
    parameter SHIP_WIDTH = 40;
    parameter SHIP_HEIGHT = 30;
    
    // 多个子弹的属性
    logic [10:0]    bullet_x[MAX_BULLETS];
    logic [9:0]     bullet_y[MAX_BULLETS];
    logic [MAX_BULLETS-1:0] bullet_active;  // 子弹活动状态位图
    parameter BULLET_SIZE = 4;
    
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
                default: begin
                    // 地址映射：
                    // 7-10: 子弹0
                    // 11-14: 子弹1
                    // ...
                    // 以及一个子弹活动位图寄存器在最后
                    if (address >= 5'd7 && address < 5'd7 + 4*MAX_BULLETS) begin
                        int bullet_idx = (address - 5'd7) / 4;  // 确定是哪个子弹
                        int bullet_reg = (address - 5'd7) % 4;  // 确定是子弹的哪个属性
                        
                        case (bullet_reg)
                            0: bullet_x[bullet_idx][7:0] <= writedata;
                            1: bullet_x[bullet_idx][10:8] <= writedata[2:0];
                            2: bullet_y[bullet_idx][7:0] <= writedata;
                            3: bullet_y[bullet_idx][9:8] <= writedata[1:0];
                        endcase
                    end
                    else if (address == 5'd7 + 4*MAX_BULLETS) begin
                        // 子弹活动状态位图
                        bullet_active <= writedata[MAX_BULLETS-1:0];
                    end
                end
            endcase
        end
    end

    // Ship display logic (triangular shape facing right)
    logic ship_on;
    assign ship_on = (hcount >= ship_x && hcount < ship_x + SHIP_WIDTH &&
                      vcount >= ship_y && vcount < ship_y + SHIP_HEIGHT) &&
                     ((hcount - ship_x) >= (SHIP_HEIGHT - (vcount - ship_y)) ||
                      (hcount - ship_x) >= ((vcount - ship_y) - SHIP_HEIGHT));

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

    // VGA output logic
    always_comb begin
        {VGA_R, VGA_G, VGA_B} = {8'h00, 8'h00, 8'h00};
        if (VGA_BLANK_n) begin
            if (ship_on)
                {VGA_R, VGA_G, VGA_B} = {8'hFF, 8'h00, 8'h00};  // Red ship
            else if (bullet_on)
                {VGA_R, VGA_G, VGA_B} = {8'hFF, 8'hFF, 8'h00};  // Yellow bullet
            else
                {VGA_R, VGA_G, VGA_B} = {background_r, background_g, background_b};
        end
    end

endmodule



// VGA timing generator module - 保持不变
module vga_counters(
    input logic        clk50, reset,
    output logic [10:0
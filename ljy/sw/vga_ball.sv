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

    output logic [7:0]  VGA_R,
    output logic [7:0]  VGA_G,
    output logic [7:0]  VGA_B,
    output logic        VGA_CLK,
    output logic        VGA_HS,
    output logic        VGA_VS,
    output logic        VGA_BLANK_n,
    output logic        VGA_SYNC_n
);

    // --------------------------------------------------
    // 参数和寄存器声明
    // --------------------------------------------------
    parameter MAX_BULLETS       = 5;
    parameter MAX_ENEMY_BULLETS = 6;
    parameter IMAGE_WIDTH       = 64;
    parameter IMAGE_HEIGHT      = 64;
    parameter SHIP_WIDTH        = 16;
    parameter SHIP_HEIGHT       = 16;
    parameter BULLET_SIZE       = 4;
    parameter ENEMY_WIDTH       = 16;
    parameter ENEMY_HEIGHT      = 16;
    parameter ENEMY_BULLET_SIZE = 4;

    // VGA 计数器输出
    logic [10:0] hcount;
    logic [ 9:0] vcount;

    // 背景色
    logic [7:0] background_r, background_g, background_b;

    // 飞船位置
    logic [10:0] ship_x;
    logic [ 9:0] ship_y;

    // 子弹
    logic [10:0]    bullet_x[MAX_BULLETS];
    logic [ 9:0]    bullet_y[MAX_BULLETS];
    logic [MAX_BULLETS-1:0] bullet_active;

    // 敌人
    logic [10:0]    enemy_x[2];
    logic [ 9:0]    enemy_y[2];
    logic [1:0]     enemy_active;

    // 敌人子弹
    logic [10:0]    enemy_bullet_x[MAX_ENEMY_BULLETS];
    logic [ 9:0]    enemy_bullet_y[MAX_ENEMY_BULLETS];
    logic [MAX_ENEMY_BULLETS-1:0] enemy_bullet_active;

    // 其它图片（如果你还要用 image_display/image_data）
    logic [10:0] image_x;
    logic [ 9:0] image_y;
    logic        image_display;
    logic [7:0]  image_data[IMAGE_HEIGHT][IMAGE_WIDTH][3];

    // --------------------------------------------------
    // 新增：ROM Sprite 接口信号
    // --------------------------------------------------
    logic [15:0] sprite_address;   // 16×16 ≤ 256 深度，用 8 位也够，扩大到16 位安全无忧
    logic [23:0] sprite_data;      // ROM 输出 24-bit RGB
    logic        sprite_on;
    logic [7:0]  sprite_r, sprite_g, sprite_b;

    // --------------------------------------------------
    // 实例化 VGA 计数器
    // --------------------------------------------------
    vga_counters counters(
        .clk50     (clk),
        .reset     (reset),
        .hcount    (hcount),
        .vcount    (vcount),
        .VGA_CLK   (VGA_CLK),
        .VGA_HS    (VGA_HS),
        .VGA_VS    (VGA_VS),
        .VGA_BLANK_n(VGA_BLANK_n),
        .VGA_SYNC_n(VGA_SYNC_n)
    );

    // --------------------------------------------------
    // 实例化你的 ROM IP
    // --------------------------------------------------
    soc_system_sprite_map sprite_map_inst (
        .address   (sprite_address),
        .clk       (clk),
        .clken     (1'b1),
        .reset_req (1'b0),
        .readdata  (sprite_data)
    );

    // --------------------------------------------------
    // Register Update Logic
    // --------------------------------------------------
    always_ff @(posedge clk) begin
        if (reset) begin
            // 背景
            background_r <= 8'h00;
            background_g <= 8'h00;
            background_b <= 8'h20;
            // 飞船位置
            ship_x <= 11'd200;
            ship_y <= 10'd240;
            // 子弹
            bullet_active <= '0;
            for (int i = 0; i < MAX_BULLETS; i++) begin
                bullet_x[i] <= 0;
                bullet_y[i] <= 0;
            end
            // 敌人
            enemy_active <= 2'b11;
            enemy_x[0] <= 11'd800; enemy_y[0] <= 10'd150;
            enemy_x[1] <= 11'd800; enemy_y[1] <= 10'd350;
            // 敌人子弹
            enemy_bullet_active <= '0;
            for (int i = 0; i < MAX_ENEMY_BULLETS; i++) begin
                enemy_bullet_x[i] <= 0;
                enemy_bullet_y[i] <= 0;
            end
            // 其它 image（如需）
            image_display <= 1'b0;
            for (int y = 0; y < IMAGE_HEIGHT; y++)
                for (int x = 0; x < IMAGE_WIDTH; x++)
                    image_data[y][x] <= '{8'd0,8'd0,8'd0};
        end
        else if (chipselect && write) begin
            case(address)
                // 背景色
                6'd0: background_r <= writedata;
                6'd1: background_g <= writedata;
                6'd2: background_b <= writedata;
                // 飞船位置
                6'd3: ship_x[7:0]  <= writedata;
                6'd4: ship_x[10:8] <= writedata[2:0];
                6'd5: ship_y[7:0]  <= writedata;
                6'd6: ship_y[9:8]  <= writedata[1:0];
                // 子弹状态
                6'd27: bullet_active <= writedata[MAX_BULLETS-1:0];
                // 敌人
                6'd28: enemy_x[0][7:0] <= writedata;
                6'd29: enemy_x[0][10:8] <= writedata[2:0];
                6'd30: enemy_y[0][7:0] <= writedata;
                6'd31: enemy_y[0][9:8] <= writedata[1:0];
                6'd32: enemy_x[1][7:0] <= writedata;
                6'd33: enemy_x[1][10:8] <= writedata[2:0];
                6'd34: enemy_y[1][7:0] <= writedata;
                6'd35: enemy_y[1][9:8] <= writedata[1:0];
                6'd36: enemy_active <= writedata[1:0];
                // 敌人子弹
                6'd61: enemy_bullet_active <= writedata[MAX_ENEMY_BULLETS-1:0];
                // 可根据需要继续映射 image_x/image_y……
                default: ;
            endcase
        end
    end

    // --------------------------------------------------
    // 计算实际像素坐标（25MHz 时钟下除 2）
    // --------------------------------------------------
    logic [10:0] actual_hcount;
    logic [ 9:0] actual_vcount;
    assign actual_hcount = {1'b0, hcount[10:1]};
    assign actual_vcount = vcount;

    // --------------------------------------------------
    // 只取 ROM 前 16×16，并拆分 RGB
    // --------------------------------------------------
    always_comb begin
        sprite_on      = 1'b0;
        sprite_address = '0;
        sprite_r = sprite_g = sprite_b = 8'd0;

        if ( actual_hcount >= ship_x &&
             actual_hcount <  ship_x + SHIP_WIDTH &&
             actual_vcount >= ship_y &&
             actual_vcount <  ship_y + SHIP_HEIGHT ) begin

            // 相对坐标
            logic [3:0] rel_x = actual_hcount - ship_x;
            logic [3:0] rel_y = actual_vcount - ship_y;
            // 线性地址到 ROM
            sprite_address = rel_y * IMAGE_WIDTH + rel_x;
            sprite_on      = 1'b1;
            // ROM 输出拆分
            {sprite_r, sprite_g, sprite_b} = sprite_data;
        end
    end

    // --------------------------------------------------
    // 原有的 image_on/ enemy_on/ bullet_on 等逻辑保持不变
    // …（省略，为简洁起见，和之前一样）
    // --------------------------------------------------

    // --------------------------------------------------
    // VGA 最终输出：Sprite 优先级最高
    // --------------------------------------------------
    always_comb begin
        // 默认黑
        {VGA_R, VGA_G, VGA_B} = {8'd0,8'd0,8'd0};

        if (VGA_BLANK_n) begin
            // 背景
            {VGA_R, VGA_G, VGA_B} = {background_r, background_g, background_b};

            // 1) ROM Sprite
            if (sprite_on) begin
                {VGA_R, VGA_G, VGA_B} = {sprite_r, sprite_g, sprite_b};
            end
            // 2) 其它游戏对象（按需保留）
            // else if (image_on)   {VGA_R,VGA_G,VGA_B} = …;
            // else if (enemy_on)   {VGA_R,VGA_G,VGA_B} = …;
            // else if (bullet_on)  {VGA_R,VGA_G,VGA_B} = …;
        end
    end

endmodule


// --------------------------------------------------
// VGA timing generator module
// --------------------------------------------------
module vga_counters(
    input  logic        clk50,
    input  logic        reset,
    output logic [10:0] hcount,
    output logic [ 9:0] vcount,
    output logic        VGA_CLK,
    output logic        VGA_HS,
    output logic        VGA_VS,
    output logic        VGA_BLANK_n,
    output logic        VGA_SYNC_n
);
    parameter HACTIVE      = 11'd1280,
              HFP          = 11'd32,
              HSYNC        = 11'd192,
              HBP          = 11'd96,
              HTOTAL       = HACTIVE + HFP + HSYNC + HBP;
    parameter VACTIVE      = 10'd480,
              VFP          = 10'd10,
              VSYNC        = 10'd2,
              VBP          = 10'd33,
              VTOTAL       = VACTIVE + VFP + VSYNC + VBP;

    logic endOfLine, endOfField;

    always_ff @(posedge clk50 or posedge reset) begin
        if (reset)      hcount <= 0;
        else if (endOfLine) hcount <= 0;
        else            hcount <= hcount + 1;
    end
    assign endOfLine  = (hcount == HTOTAL - 1);

    always_ff @(posedge clk50 or posedge reset) begin
        if (reset)      vcount <= 0;
        else if (endOfLine) begin
            if (endOfField) vcount <= 0;
            else            vcount <= vcount + 1;
        end
    end
    assign endOfField = (vcount == VTOTAL - 1);

    // HSYNC/VSYNC/BLANK
    assign VGA_HS     = !((hcount>=HACTIVE+HFP) && (hcount<HACTIVE+HFP+HSYNC));
    assign VGA_VS     = !((vcount>=VACTIVE+VFP) && (vcount<VACTIVE+VFP+VSYNC));
    assign VGA_BLANK_n= (hcount<HACTIVE) && (vcount<VACTIVE);
    assign VGA_SYNC_n = 1'b0;
    // 25 MHz 时钟
    assign VGA_CLK    = clk50;

endmodule

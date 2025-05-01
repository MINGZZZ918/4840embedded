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
    // 参数声明
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

    // --------------------------------------------------
    // 信号声明
    // --------------------------------------------------
    // VGA 计数器输出
    logic [10:0] hcount;
    logic [ 9:0] vcount;

    // 背景色
    logic [7:0] background_r, background_g, background_b;

    // 飞船位置
    logic [10:0] ship_x;
    logic [ 9:0] ship_y;

    // 玩家子弹
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

    // 其它图片（如果需要）
    logic [10:0] image_x;
    logic [ 9:0] image_y;
    logic        image_display;
    logic [7:0]  image_data[IMAGE_HEIGHT][IMAGE_WIDTH][3];

    // --------------------------------------------------
    // ROM Sprite 接口
    // --------------------------------------------------
    logic [15:0] sprite_address;   // 用于寻址 ROM 中前 16×16
    logic [23:0] sprite_data;      // ROM 输出 24-bit RGB
    logic        sprite_on;
    logic [7:0]  sprite_r, sprite_g, sprite_b;

    // --------------------------------------------------
    // 实例化 VGA 计数器
    // --------------------------------------------------
    vga_counters counters(
        .clk50      (clk),
        .reset      (reset),
        .hcount     (hcount),
        .vcount     (vcount),
        .VGA_CLK    (VGA_CLK),
        .VGA_HS     (VGA_HS),
        .VGA_VS     (VGA_VS),
        .VGA_BLANK_n(VGA_BLANK_n),
        .VGA_SYNC_n (VGA_SYNC_n)
    );

    // --------------------------------------------------
    // 实例化 ROM IP
    // --------------------------------------------------
    soc_system_sprite_map sprite_map_inst (
        .address   (sprite_address),
        .clk       (clk),
        .clken     (1'b1),
        .reset_req (1'b0),
        .readdata  (sprite_data)
    );

    // --------------------------------------------------
    // 寄存器更新逻辑
    // --------------------------------------------------
    always_ff @(posedge clk) begin
        if (reset) begin
            // 背景色
            background_r <= 8'h00;
            background_g <= 8'h00;
            background_b <= 8'h20;
            // 飞船初始位置
            ship_x <= 11'd200;
            ship_y <= 10'd240;
            // 玩家子弹
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
            // 其它图片
            image_display <= 1'b0;
            for (int y = 0; y < IMAGE_HEIGHT; y++)
                for (int x = 0; x < IMAGE_WIDTH; x++)
                    image_data[y][x] <= '{8'd0,8'd0,8'd0};
        end
        else if (chipselect && write) begin
            case(address)
                // 背景色
                6'd0: background_r       <= writedata;
                6'd1: background_g       <= writedata;
                6'd2: background_b       <= writedata;
                // 飞船位置
                6'd3: ship_x[7:0]        <= writedata;
                6'd4: ship_x[10:8]       <= writedata[2:0];
                6'd5: ship_y[7:0]        <= writedata;
                6'd6: ship_y[9:8]        <= writedata[1:0];
                // 玩家子弹激活位图
                6'd27: bullet_active    <= writedata[MAX_BULLETS-1:0];
                // 敌人位置 & 状态
                6'd28: enemy_x[0][7:0]   <= writedata;
                6'd29: enemy_x[0][10:8]  <= writedata[2:0];
                6'd30: enemy_y[0][7:0]   <= writedata;
                6'd31: enemy_y[0][9:8]   <= writedata[1:0];
                6'd32: enemy_x[1][7:0]   <= writedata;
                6'd33: enemy_x[1][10:8]  <= writedata[2:0];
                6'd34: enemy_y[1][7:0]   <= writedata;
                6'd35: enemy_y[1][9:8]   <= writedata[1:0];
                6'd36: enemy_active      <= writedata[1:0];
                // 敌人子弹激活位图
                6'd61: enemy_bullet_active <= writedata[MAX_ENEMY_BULLETS-1:0];
                // 其余寄存器……
                default: ;
            endcase
        end
    end

    // --------------------------------------------------
    // 计算实际像素坐标（25MHz 时钟下 hcount 除 2）
    // --------------------------------------------------
    logic [10:0] actual_hcount = {1'b0, hcount[10:1]};
    logic [ 9:0] actual_vcount = vcount;

    // --------------------------------------------------
    // Sprite (ROM) 显示逻辑：只取前 16×16 像素
    // --------------------------------------------------
    always_comb begin
        sprite_on      = 1'b0;
        sprite_address = '0;
        sprite_r = 8'd0;
        sprite_g = 8'd0;
        sprite_b = 8'd0;

        if ( actual_hcount >= ship_x &&
             actual_hcount <  ship_x + SHIP_WIDTH &&
             actual_vcount >= ship_y &&
             actual_vcount <  ship_y + SHIP_HEIGHT ) begin

            logic [3:0] rel_x = actual_hcount - ship_x;
            logic [3:0] rel_y = actual_vcount - ship_y;
            sprite_address = rel_y * IMAGE_WIDTH + rel_x;
            sprite_on      = 1'b1;
            {sprite_r, sprite_g, sprite_b} = sprite_data;
        end
    end

    // --------------------------------------------------
    // 原有的 image_on 逻辑
    // --------------------------------------------------
    logic image_on;
    logic [7:0] image_pixel_r, image_pixel_g, image_pixel_b;
    logic [5:0] img_x, img_y;
    always_comb begin
        image_on = 1'b0;
        image_pixel_r = 8'd0;
        image_pixel_g = 8'd0;
        image_pixel_b = 8'd0;
        if (image_display &&
            actual_hcount >= image_x && actual_hcount <  image_x + IMAGE_WIDTH &&
            actual_vcount >= image_y && actual_vcount <  image_y + IMAGE_HEIGHT) begin

            img_x = actual_hcount - image_x;
            img_y = actual_vcount - image_y;
            if (img_x < IMAGE_WIDTH && img_y < IMAGE_HEIGHT) begin
                image_pixel_r = image_data[img_y][img_x][0];
                image_pixel_g = image_data[img_y][img_x][1];
                image_pixel_b = image_data[img_y][img_x][2];
                if (image_pixel_r!=0 || image_pixel_g!=0 || image_pixel_b!=0)
                    image_on = 1'b1;
            end
        end
    end

    // --------------------------------------------------
    // 原有的 enemy_on 逻辑
    // --------------------------------------------------
    logic        enemy_on;
    logic [1:0]  enemy_pixel_value;
    logic [3:0]  enemy_rel_x, enemy_rel_y;
    always_comb begin
        enemy_on = 1'b0;
        enemy_pixel_value = 2'b00;
        for (int i = 0; i < 2; i++) begin
            if (enemy_active[i] &&
                actual_hcount >= enemy_x[i] && actual_hcount < enemy_x[i]+ENEMY_WIDTH &&
                actual_vcount >= enemy_y[i] && actual_vcount < enemy_y[i]+ENEMY_HEIGHT) begin

                enemy_rel_x = actual_hcount - enemy_x[i];
                enemy_rel_y = actual_vcount - enemy_y[i];
                if (enemy_rel_x<ENEMY_WIDTH && enemy_rel_y<ENEMY_HEIGHT) begin
                    // 这里原来是从 enemy_pattern 取值，暂不使用
                    // 直接显示白色或绿色
                    enemy_pixel_value = 2'b01;
                    enemy_on = 1'b1;
                end
            end
        end
    end

    // --------------------------------------------------
    // 原有的 bullet_on 逻辑
    // --------------------------------------------------
    logic bullet_on;
    always_comb begin
        bullet_on = 1'b0;
        for (int i = 0; i < MAX_BULLETS; i++) begin
            if (bullet_active[i] &&
                actual_hcount >= bullet_x[i] && actual_hcount < bullet_x[i]+BULLET_SIZE &&
                actual_vcount >= bullet_y[i] && actual_vcount < bullet_y[i]+BULLET_SIZE) begin
                bullet_on = 1'b1;
            end
        end
    end

    // --------------------------------------------------
    // 原有的 enemy_bullet_on 逻辑
    // --------------------------------------------------
    logic enemy_bullet_on;
    always_comb begin
        enemy_bullet_on = 1'b0;
        for (int i = 0; i < MAX_ENEMY_BULLETS; i++) begin
            if (enemy_bullet_active[i] &&
                actual_hcount >= enemy_bullet_x[i] && actual_hcount < enemy_bullet_x[i]+ENEMY_BULLET_SIZE &&
                actual_vcount >= enemy_bullet_y[i] && actual_vcount < enemy_bullet_y[i]+ENEMY_BULLET_SIZE) begin
                enemy_bullet_on = 1'b1;
            end
        end
    end

    // --------------------------------------------------
    // VGA 输出：Sprite 优先级最高
    // --------------------------------------------------
    always_comb begin
        // 默认为背景色
        {VGA_R, VGA_G, VGA_B} = {background_r, background_g, background_b};

        if (VGA_BLANK_n) begin
            if (sprite_on) begin
                {VGA_R, VGA_G, VGA_B} = {sprite_r, sprite_g, sprite_b};
            end
            else if (image_on) begin
                {VGA_R, VGA_G, VGA_B} = {image_pixel_r, image_pixel_g, image_pixel_b};
            end
            else if (enemy_on) begin
                {VGA_R, VGA_G, VGA_B} = {8'h20, 8'hE0, 8'h20};
            end
            else if (bullet_on) begin
                {VGA_R, VGA_G, VGA_B} = {8'hFF, 8'hFF, 8'h00};
            end
            else if (enemy_bullet_on) begin
                {VGA_R, VGA_G, VGA_B} = {8'hFF, 8'h40, 8'h00};
            end
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
    // 水平参数
    parameter HACTIVE = 11'd1280,
              HFP     = 11'd32,
              HSYNC   = 11'd192,
              HBP     = 11'd96,
              HTOTAL  = HACTIVE+HFP+HSYNC+HBP;
    // 垂直参数
    parameter VACTIVE = 10'd480,
              VFP     = 10'd10,
              VSYNC   = 10'd2,
              VBP     = 10'd33,
              VTOTAL  = VACTIVE+VFP+VSYNC+VBP;

    logic endOfLine, endOfField;

    always_ff @(posedge clk50 or posedge reset) begin
        if (reset)            hcount <= 0;
        else if (endOfLine)   hcount <= 0;
        else                  hcount <= hcount + 1;
    end
    assign endOfLine = (hcount == HTOTAL-1);

    always_ff @(posedge clk50 or posedge reset) begin
        if (reset)            vcount <= 0;
        else if (endOfLine) begin
            if (endOfField)   vcount <= 0;
            else              vcount <= vcount + 1;
        end
    end
    assign endOfField = (vcount == VTOTAL-1);

    // HSYNC / VSYNC / BLANK
    assign VGA_HS      = !((hcount>=HACTIVE+HFP) && (hcount<HACTIVE+HFP+HSYNC));
    assign VGA_VS      = !((vcount>=VACTIVE+VFP) && (vcount<VACTIVE+VFP+VSYNC));
    assign VGA_BLANK_n = (hcount<HACTIVE) && (vcount<VACTIVE);
    assign VGA_SYNC_n  = 1'b0;
    // 25 MHz 直接用 clk50
    assign VGA_CLK     = clk50;

endmodule

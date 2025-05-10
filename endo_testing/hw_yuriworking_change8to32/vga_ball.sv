/*
 * Avalon memory-mapped peripheral for VGA Ball Game
 */

module vga_ball#(
        parameter MAX_OBJECTS = 20
)
    input  logic        clk,
    input  logic        reset,
    input  logic [31:0] writedata,
    input  logic        write,
    input  logic        chipselect,
    input  logic [5:0]  address,

    output logic [7:0]  VGA_R, VGA_G, VGA_B,
    output logic        VGA_CLK, VGA_HS, VGA_VS,
                        VGA_BLANK_n,
    output logic        VGA_SYNC_n
);

    // 常量定义
    parameter IMAGE_WIDTH = 64;  // 图片宽度
    parameter IMAGE_HEIGHT = 64; // 图片高度
    parameter MAX_OBJECTS = 20;

    logic [10:0]    hcount;
    logic [9:0]     vcount;

    // Background color
    logic [7:0]     background_r, background_g, background_b;
    
    // Spaceship position and properties
    logic [10:0]    ship_x;
    logic [9:0]     ship_y;
    parameter SHIP_WIDTH = 16;   // 飞船宽度
    parameter SHIP_HEIGHT = 16;  // 飞船高度
    
    // 图片属性
    logic [10:0]    image_x;
    logic [9:0]     image_y;
    logic           image_display;
    logic [7:0]     image_data[IMAGE_HEIGHT][IMAGE_WIDTH][3]; // RGB数据

    // 飞船像素艺术模式 (16x16)
    // 0=黑色(透明), 1=红色, 2=白色, 3=蓝色
    logic [1:0] ship_pattern[16][16]; // 2位宽，支持4种颜色
    

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

    // === Updated 32-bit writedata interface ===
    logic [11:0] obj_x   [MAX_OBJECTS];
    logic [11:0] obj_y   [MAX_OBJECTS];
    logic [5:0]  obj_sprite [MAX_OBJECTS];
    logic        obj_active [MAX_OBJECTS];

    always_ff @(posedge clk) begin
        if (reset) begin
            // 初始化背景色
            background_r <= 8'h00;
            background_g <= 8'h80;
            background_b <= 8'h00;

            // 初始化所有对象
            for (int i = 0; i < MAX_OBJECTS; i++) begin
                obj_x[i]      <= 12'd0;
                obj_y[i]      <= 12'd0;
                obj_sprite[i] <= 6'd0;
                obj_active[i] <= 1'b0;
            end

            // Example: 默认放置船和两个敌人
            obj_x[0]      <= 12'd200;                  // ship
            obj_y[0]      <= 12'd240;
            obj_sprite[0] <= SHIP_SPRITE_INDEX;
            obj_active[0] <= 1'b1;

            obj_x[1]      <= 12'd800;                  // enemy #1
            obj_y[1]      <= 12'd150;
            obj_sprite[1] <= ENEMY_SPRITE_START;
            obj_active[1] <= 1'b1;

            obj_x[2]      <= 12'd800;                  // enemy #2
            obj_y[2]      <= 12'd350;
            obj_sprite[2] <= ENEMY_SPRITE_START;
            obj_active[2] <= 1'b1;
        end
        else if (chipselect && write) begin
            case (address)
                // 背景色写入：24位
                5'd0:
                    {background_r, background_g, background_b}
                    <= writedata[23:0];

                // 对象数据写入：x[11:0], y[11:0], sprite[5:0], active[1]
                default: begin
                    if (address >= 5'd1
                     && address <  5'd1 + MAX_OBJECTS) begin
                        int obj_idx = address - 5'd1;
                        obj_x[obj_idx]      <= writedata[31:20];
                        obj_y[obj_idx]      <= writedata[19:8];
                        obj_sprite[obj_idx] <= writedata[7:2];
                        obj_active[obj_idx] <= writedata[1];
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

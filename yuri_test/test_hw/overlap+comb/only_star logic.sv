// /*
//  * Avalon memory-mapped peripheral for VGA Ball Game
//  */

// module vga_ball#(
//     parameter STAR_COUNT   = 64
// ) (
//     input  logic        clk,
//     input  logic        reset,
//     input  logic [31:0] writedata,  // 改为32位宽度
//     input  logic        write,
//     input  logic        chipselect,
//     input  logic [6:0]  address,    // 由于一次传32位，地址空间可以减小
//     output logic [7:0]  VGA_R, VGA_G, VGA_B,
//     output logic        VGA_CLK, VGA_HS, VGA_VS,
//     output logic        VGA_BLANK_n,
//     output logic        VGA_SYNC_n
// );

//     // 常量定义
//     logic [10:0]    hcount;
//     logic [9:0]     vcount;

//     // Background color
//     logic [7:0]     background_r, background_g, background_b;
    

//     // Instantiate VGA counter module
//     assign background_r = 8'h00;
//     assign background_g = 8'h00;
//     assign background_b = 8'h40;  // 深夜蓝
//     vga_counters counters(.clk50(clk), .*);

//     // 星星坐标阵列（复位时通过 seed_lfsr 填充，一旦确定后不再改变）
//     logic [10:0] star_x [0:STAR_COUNT-1];
//     logic [ 9:0] star_y [0:STAR_COUNT-1];

//     // 用于复位时初始化星星坐标的 LFSR
//     logic [15:0] seed_lfsr;

//     // 用于每帧闪烁判定的主 LFSR
//     logic [15:0] flicker_lfsr;

//     // 当前像素是否为“点亮的星星”
//     logic star_pixel;

//     // ———— LFSR 逻辑 ————
//     // 复位时填充 star_x/star_y，之后每帧推进 flicker_lfsr
//     always_ff @(posedge clk or posedge reset) begin
//     if (reset) begin
//         // 用阻塞赋值
//         seed_lfsr    = 16'hBEEF;
//         flicker_lfsr = 16'hACE1;
//         for (int i = 0; i < STAR_COUNT; i++) begin
//         // 取低 11 位做 X
//         star_x[i] = seed_lfsr[10:0];
//         // 推进 LFSR —— 阻塞赋
//         seed_lfsr = {seed_lfsr[0] ^ seed_lfsr[2] ^ seed_lfsr[3] ^ seed_lfsr[5],
//                     seed_lfsr[15:1]};
//         // 取低 10 位做 Y
//         star_y[i] = seed_lfsr[9:0];
//         // 再推进一次
//         seed_lfsr = {seed_lfsr[0] ^ seed_lfsr[2] ^ seed_lfsr[3] ^ seed_lfsr[5],
//                     seed_lfsr[15:1]};
//         end
//     end
//     else if (vcount == 0 && hcount == 0) begin
//         // 每帧推进非阻塞就行
//         flicker_lfsr <= {flicker_lfsr[0] ^ flicker_lfsr[2] ^ flicker_lfsr[3] ^ flicker_lfsr[5],
//                         flicker_lfsr[15:1]};
//     end
//     end

//     // ----------------------------------------------------------------
//     // 呼吸灯计数器：每帧更新一次
//     // ----------------------------------------------------------------
//     logic [7:0] breath_level;
//     logic       breath_up;

//     always_ff @(posedge clk or posedge reset) begin
//         if (reset) begin
//             breath_level <= 8'd0;
//             breath_up    <= 1'b1;
//         end
//         else if (vcount == 0 && hcount[10:1] == 0) begin
//             // 每帧第一个像素，用它做时钟
//             if (breath_up) begin
//                 breath_level <= breath_level + 1;
//                 if (breath_level == 8'hFF) breath_up <= 1'b0;
//             end else begin
//                 breath_level <= breath_level - 1;
//                 if (breath_level == 8'd0  ) breath_up <= 1'b1;
//             end
//         end
//     end


//     // ———— 星星点亮判定 ————
//     // 如果当前像素刚好是某颗星的位置，且该星的 flicker 位为 1，则点亮
//     always_comb begin
//         star_pixel = 1'b0;
//         for (int i = 0; i < STAR_COUNT; i++) begin
//             if (hcount == star_x[i]
//              && vcount  == star_y[i]
//              && flicker_lfsr[i % 16]) begin
//                 star_pixel = 1'b1;
//             end
//         end
//     end

//     // 渲染逻辑 - 确定当前像素属于哪个对象

//     always_comb begin
//             VGA_R = background_r;
//             VGA_G = background_g;
//             VGA_B = background_b;
//         if (star_pixel) begin
//             // 星星采用亮黄色
//             // 用呼吸级别去调节一个橙黄色 (0xFFA0) 的幅度
//             VGA_R = (8'hFF * breath_level) >> 8;
//             VGA_G = (8'hF0 * breath_level) >> 8;
//             VGA_B = (8'hA0 * breath_level) >> 8;
//         end
//         else begin
//             // 普通背景
//             VGA_R = background_r;
//             VGA_G = background_g;
//             VGA_B = background_b;
//         end
//     end
    
// endmodule


// // VGA timing generator module
// module vga_counters(
//     input logic        clk50, reset,
//     output logic [10:0] hcount,  // hcount是像素列，hcount[10:1]是实际显示的像素位置
//     output logic [9:0]  vcount,  // vcount是像素行
//     output logic        VGA_CLK, VGA_HS, VGA_VS, VGA_BLANK_n, VGA_SYNC_n
// );

//     // Parameters for hcount
//     parameter HACTIVE      = 11'd 1280,
//               HFRONT_PORCH = 11'd 32,
//               HSYNC        = 11'd 192,
//               HBACK_PORCH  = 11'd 96,   
//               HTOTAL       = HACTIVE + HFRONT_PORCH + HSYNC + HBACK_PORCH; // 1600
    
//     // Parameters for vcount
//     parameter VACTIVE      = 10'd 480,
//               VFRONT_PORCH = 10'd 10,
//               VSYNC        = 10'd 2,
//               VBACK_PORCH  = 10'd 33,
//               VTOTAL       = VACTIVE + VFRONT_PORCH + VSYNC + VBACK_PORCH; // 525

//     logic endOfLine;
    
//     always_ff @(posedge clk50 or posedge reset)
//         if (reset)          hcount <= 0;
//         else if (endOfLine) hcount <= 0;
//         else                hcount <= hcount + 11'd 1;

//     assign endOfLine = hcount == HTOTAL - 1;
        
//     logic endOfField;
    
//     always_ff @(posedge clk50 or posedge reset)
//         if (reset)          vcount <= 0;
//         else if (endOfLine)
//             if (endOfField) vcount <= 0;
//             else            vcount <= vcount + 10'd 1;

//     assign endOfField = vcount == VTOTAL - 1;

//     // Horizontal sync: from 0x520 to 0x5DF (0x57F)
//     assign VGA_HS = !( (hcount[10:8] == 3'b101) & !(hcount[7:5] == 3'b111));
//     assign VGA_VS = !( vcount[9:1] == (VACTIVE + VFRONT_PORCH) / 2);

//     assign VGA_SYNC_n = 1'b0; // For putting sync on the green signal; unused
    
//     // Horizontal active: 0 to 1279     Vertical active: 0 to 479
//     assign VGA_BLANK_n = !( hcount[10] & (hcount[9] | hcount[8]) ) &
//                         !( vcount[9] | (vcount[8:5] == 4'b1111) );

//     assign VGA_CLK = hcount[0]; // 25 MHz clock: rising edge sensitive
    
// endmodule

/*
 * Avalon memory-mapped peripheral for VGA Ball Game
 */
module vga_ball #(
    parameter STAR_COUNT = 64
) (
    input  logic        clk,
    input  logic        reset,
    input  logic [31:0] writedata,  // 改为32位宽度
    input  logic        write,
    input  logic        chipselect,
    input  logic [6:0]  address,    // 由于一次传32位，地址空间可以减小
    output logic [7:0]  VGA_R,
    output logic [7:0]  VGA_G,
    output logic [7:0]  VGA_B,
    output logic        VGA_CLK,
    output logic        VGA_HS,
    output logic        VGA_VS,
    output logic        VGA_BLANK_n,
    output logic        VGA_SYNC_n
);

    // VGA 时序计数器
    logic [10:0] hcount;
    logic [9:0]  vcount;

    // 背景色：深夜蓝
    logic [7:0] background_r = 8'h00;
    logic [7:0] background_g = 8'h00;
    logic [7:0] background_b = 8'h40;

    // 实例化 VGA 计数器
    vga_counters counters (
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

    // 星星坐标阵列
    logic [10:0] star_x [0:STAR_COUNT-1];
    logic [ 9:0] star_y [0:STAR_COUNT-1];

    // 用于生成 X、Y 坐标的两个独立 LFSR
    logic [15:0] seed_lfsr_x, seed_lfsr_y;
    // 用于闪烁的主 LFSR
    logic [15:0] flicker_lfsr;
    // 6 帧计数器
    logic [2:0]  frame_cnt;

    // 复位时初始化星星坐标；否则每6帧推进一次 flicker_lfsr
    always_ff @(posedge clk or posedge reset) begin
        if (reset) begin
            seed_lfsr_x   <= 16'hBEEF;
            seed_lfsr_y   <= 16'hACE1;
            flicker_lfsr  <= 16'hFACE;
            frame_cnt     <= 3'd0;
            // 填充 STAR_COUNT 个随机坐标
            for (int i = 0; i < STAR_COUNT; i++) begin
                star_x[i] <= seed_lfsr_x[10:0];
                seed_lfsr_x <= {seed_lfsr_x[0]^seed_lfsr_x[2]^seed_lfsr_x[3]^seed_lfsr_x[5],
                                seed_lfsr_x[15:1]};
                star_y[i] <= seed_lfsr_y[9:0];
                seed_lfsr_y <= {seed_lfsr_y[0]^seed_lfsr_y[2]^seed_lfsr_y[3]^seed_lfsr_y[5],
                                seed_lfsr_y[15:1]};
            end
        end
        else if (vcount == 0 && hcount == 0) begin
            if (frame_cnt == 3'd5) begin
                // 每第6帧推进一次
                flicker_lfsr <= {flicker_lfsr[0]^flicker_lfsr[2]^flicker_lfsr[3]^flicker_lfsr[5],
                                 flicker_lfsr[15:1]};
                frame_cnt    <= 3'd0;
            end else begin
                frame_cnt <= frame_cnt + 3'd1;
            end
        end
    end

    // 判断当前像素是否为“点亮的星星”
    logic star_pixel;
    always_comb begin
        star_pixel = 1'b0;
        for (int i = 0; i < STAR_COUNT; i++) begin
            if (hcount == star_x[i]
             && vcount == star_y[i]
             && flicker_lfsr[i % 16]) begin
                star_pixel = 1'b1;
            end
        end
    end

    // 渲染逻辑：背景或亮黄色星星
    always_comb begin
        VGA_R = background_r;
        VGA_G = background_g;
        VGA_B = background_b;
        if (star_pixel) begin
            VGA_R = 8'hFF;  // 亮黄色
            VGA_G = 8'hFF;
            VGA_B = 8'h00;
        end
    end

endmodule


// VGA timing generator module
module vga_counters(
    input  logic        clk50,
    input  logic        reset,
    output logic [10:0] hcount,    // 水平像素计数
    output logic [ 9:0] vcount,    // 垂直像素计数
    output logic        VGA_CLK,
    output logic        VGA_HS,
    output logic        VGA_VS,
    output logic        VGA_BLANK_n,
    output logic        VGA_SYNC_n
);

    // 水平周期参数（1280×480 @60Hz）
    parameter HACTIVE      = 11'd1280,
              HFRONT_PORCH = 11'd32,
              HSYNC        = 11'd192,
              HBACK_PORCH  = 11'd96,
              HTOTAL       = HACTIVE + HFRONT_PORCH + HSYNC + HBACK_PORCH; // 1600

    // 垂直周期参数
    parameter VACTIVE      = 10'd480,
              VFRONT_PORCH = 10'd10,
              VSYNC        = 10'd2,
              VBACK_PORCH  = 10'd33,
              VTOTAL       = VACTIVE + VFRONT_PORCH + VSYNC + VBACK_PORCH; // 525

    logic endOfLine, endOfField;

    // 水平计数
    always_ff @(posedge clk50 or posedge reset) begin
        if (reset)          hcount <= 11'd0;
        else if (endOfLine) hcount <= 11'd0;
        else                hcount <= hcount + 11'd1;
    end
    assign endOfLine = (hcount == HTOTAL - 1);

    // 垂直计数
    always_ff @(posedge clk50 or posedge reset) begin
        if (reset)          vcount <= 10'd0;
        else if (endOfLine) begin
            if (endOfField) vcount <= 10'd0;
            else            vcount <= vcount + 10'd1;
        end
    end
    assign endOfField = (vcount == VTOTAL - 1);

    // 同步信号
    assign VGA_HS     = !((hcount >= (HACTIVE + HFRONT_PORCH)) &&
                         (hcount <  (HACTIVE + HFRONT_PORCH + HSYNC)));
    assign VGA_VS     = !((vcount >= (VACTIVE + VFRONT_PORCH)) &&
                         (vcount <  (VACTIVE + VFRONT_PORCH + VSYNC)));
    // 合成 sync 到绿信号线，通常未用
    assign VGA_SYNC_n = 1'b0;
    // 有效显示区 blank 信号
    assign VGA_BLANK_n = !((hcount < HACTIVE) && (vcount < VACTIVE));
    // 25MHz 像素时钟
    assign VGA_CLK = hcount[0];

endmodule
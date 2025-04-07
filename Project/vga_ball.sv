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
 *        7     |  Bullet X position (lower 8 bits)
 *        8     |  Bullet X position (upper 3 bits)
 *        9     |  Bullet Y position (lower 8 bits)
 *        10    |  Bullet Y position (upper 2 bits)
 *        11    |  Bullet Active (1 = active, 0 = inactive)
 */

module vga_ball(
    input  logic        clk,
    input  logic        reset,
    input  logic [7:0]  writedata,
    input  logic        write,
    input  logic        chipselect,
    input  logic [3:0]  address,

    output logic [7:0]  VGA_R, VGA_G, VGA_B,
    output logic        VGA_CLK, VGA_HS, VGA_VS,
                        VGA_BLANK_n,
    output logic        VGA_SYNC_n
);

    logic [10:0]    hcount;
    logic [9:0]     vcount;

    // Background color
    logic [7:0]     background_r, background_g, background_b;
    
    // Spaceship position and properties
    logic [10:0]    ship_x;
    logic [9:0]     ship_y;
    parameter SHIP_WIDTH = 40;
    parameter SHIP_HEIGHT = 30;
    
    // Bullet properties
    logic [10:0]    bullet_x;
    logic [9:0]     bullet_y;
    logic           bullet_active;
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
            ship_x <= 11'd200;      // Ship position
            ship_y <= 10'd240;      // Middle of screen vertically
            
            bullet_x <= 11'd0;
            bullet_y <= 10'd0;
            bullet_active <= 1'b0;  // Inactive by default
        end 
        else if (chipselect && write) begin
            case (address)
                4'd0: background_r <= writedata;
                4'd1: background_g <= writedata;
                4'd2: background_b <= writedata;
                
                // Ship position
                4'd3: ship_x[7:0] <= writedata;
                4'd4: ship_x[10:8] <= writedata[2:0];
                4'd5: ship_y[7:0] <= writedata;
                4'd6: ship_y[9:8] <= writedata[1:0];
                
                // Bullet properties
                4'd7: bullet_x[7:0] <= writedata;
                4'd8: bullet_x[10:8] <= writedata[2:0];
                4'd9: bullet_y[7:0] <= writedata;
                4'd10: bullet_y[9:8] <= writedata[1:0];
                4'd11: bullet_active <= writedata[0];
            endcase
        end
    end

    // Ship display logic (triangular shape facing right)
    logic ship_on;
    assign ship_on = (hcount >= ship_x && hcount < ship_x + SHIP_WIDTH &&
                      vcount >= ship_y && vcount < ship_y + SHIP_HEIGHT) &&
                     ((hcount - ship_x) >= (SHIP_HEIGHT - (vcount - ship_y)) ||
                      (hcount - ship_x) >= ((vcount - ship_y) - SHIP_HEIGHT));

    // Bullet display logic (small square)
    logic bullet_on;
    assign bullet_on = bullet_active && 
                       (hcount >= bullet_x && hcount < bullet_x + BULLET_SIZE &&
                        vcount >= bullet_y && vcount < bullet_y + BULLET_SIZE);

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



// VGA timing generator module
module vga_counters(
    input logic        clk50, reset,
    output logic [10:0] hcount,  // hcount[10:1] is pixel column
    output logic [9:0]  vcount,  // vcount[9:0] is pixel row
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
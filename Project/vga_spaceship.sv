/*
 * Avalon memory-mapped peripheral for Space Shooter Game
 *
 * Register Map:
 * 
 * Byte Offset    Meaning
 *        0     |  Background Red
 *        1     |  Background Green
 *        2     |  Background Blue
 *        3     |  Player1 X position (lower 8 bits)
 *        4     |  Player1 X position (upper 3 bits)
 *        5     |  Player1 Y position (lower 8 bits)
 *        6     |  Player1 Y position (upper 2 bits)
 *        7     |  Player2 X position (lower 8 bits)
 *        8     |  Player2 X position (upper 3 bits)
 *        9     |  Player2 Y position (lower 8 bits)
 *       10     |  Player2 Y position (upper 2 bits)
 *       11     |  Bullet1 X position (lower 8 bits)
 *       12     |  Bullet1 X position (upper 3 bits)
 *       13     |  Bullet1 Y position (lower 8 bits)
 *       14     |  Bullet1 Y position (upper 2 bits)
 *       15     |  Bullet1 Active (1 = active, 0 = inactive)
 *       16     |  Bullet2 X position (lower 8 bits)
 *       17     |  Bullet2 X position (upper 3 bits)
 *       18     |  Bullet2 Y position (lower 8 bits)
 *       19     |  Bullet2 Y position (upper 2 bits)
 *       20     |  Bullet2 Active (1 = active, 0 = inactive)
 */

module vga_spaceship(
    input  logic        clk,
    input  logic        reset,
    input  logic [7:0]  writedata,
    input  logic        write,
    input  logic        chipselect,
    input  logic [4:0]  address,

    output logic [7:0]  VGA_R, VGA_G, VGA_B,
    output logic        VGA_CLK, VGA_HS, VGA_VS,
                        VGA_BLANK_n,
    output logic        VGA_SYNC_n
);

    logic [10:0]    hcount;
    logic [9:0]     vcount;

    // Background color
    logic [7:0]     background_r, background_g, background_b;
    
    // Spaceship 1 position and properties
    logic [10:0]    ship1_x;
    logic [9:0]     ship1_y;
    parameter SHIP_WIDTH = 40;
    parameter SHIP_HEIGHT = 30;
    
    // Spaceship 2 position
    logic [10:0]    ship2_x;
    logic [9:0]     ship2_y;
    
    // Bullet 1 properties
    logic [10:0]    bullet1_x;
    logic [9:0]     bullet1_y;
    logic           bullet1_active;
    parameter BULLET_SIZE = 4;
    
    // Bullet 2 properties
    logic [10:0]    bullet2_x;
    logic [9:0]     bullet2_y;
    logic           bullet2_active;
    
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
            ship1_x <= 11'd200;     // Left ship position
            ship1_y <= 10'd240;     // Middle of screen vertically
            
            ship2_x <= 11'd1000;    // Right ship position
            ship2_y <= 10'd240;     // Middle of screen vertically
            
            bullet1_x <= 11'd0;
            bullet1_y <= 10'd0;
            bullet1_active <= 1'b0; // Inactive by default
            
            bullet2_x <= 11'd0;
            bullet2_y <= 10'd0;
            bullet2_active <= 1'b0; // Inactive by default
        end 
        else if (chipselect && write) begin
            case (address)
                5'd0: background_r <= writedata;
                5'd1: background_g <= writedata;
                5'd2: background_b <= writedata;
                
                // Ship 1 position
                5'd3: ship1_x[7:0] <= writedata;
                5'd4: ship1_x[10:8] <= writedata[2:0];
                5'd5: ship1_y[7:0] <= writedata;
                5'd6: ship1_y[9:8] <= writedata[1:0];
                
                // Ship 2 position
                5'd7: ship2_x[7:0] <= writedata;
                5'd8: ship2_x[10:8] <= writedata[2:0];
                5'd9: ship2_y[7:0] <= writedata;
                5'd10: ship2_y[9:8] <= writedata[1:0];
                
                // Bullet 1 properties
                5'd11: bullet1_x[7:0] <= writedata;
                5'd12: bullet1_x[10:8] <= writedata[2:0];
                5'd13: bullet1_y[7:0] <= writedata;
                5'd14: bullet1_y[9:8] <= writedata[1:0];
                5'd15: bullet1_active <= writedata[0];
                
                // Bullet 2 properties
                5'd16: bullet2_x[7:0] <= writedata;
                5'd17: bullet2_x[10:8] <= writedata[2:0];
                5'd18: bullet2_y[7:0] <= writedata;
                5'd19: bullet2_y[9:8] <= writedata[1:0];
                5'd20: bullet2_active <= writedata[0];
            endcase
        end
    end

    // Ship 1 display logic (triangular shape facing right)
    logic ship1_on;
    assign ship1_on = (hcount >= ship1_x && hcount < ship1_x + SHIP_WIDTH &&
                       vcount >= ship1_y && vcount < ship1_y + SHIP_HEIGHT) &&
                      ((hcount - ship1_x) >= (SHIP_HEIGHT - (vcount - ship1_y)) ||
                       (hcount - ship1_x) >= ((vcount - ship1_y) - SHIP_HEIGHT));

    // Ship 2 display logic (triangular shape facing left)
    logic ship2_on;
    assign ship2_on = (hcount >= ship2_x && hcount < ship2_x + SHIP_WIDTH &&
                       vcount >= ship2_y && vcount < ship2_y + SHIP_HEIGHT) &&
                      ((ship2_x + SHIP_WIDTH - hcount) >= (SHIP_HEIGHT - (vcount - ship2_y)) ||
                       (ship2_x + SHIP_WIDTH - hcount) >= ((vcount - ship2_y) - SHIP_HEIGHT));

    // Bullet 1 display logic (small square)
    logic bullet1_on;
    assign bullet1_on = bullet1_active && 
                        (hcount >= bullet1_x && hcount < bullet1_x + BULLET_SIZE &&
                        vcount >= bullet1_y && vcount < bullet1_y + BULLET_SIZE);

    // Bullet 2 display logic (small square)
    logic bullet2_on;
    assign bullet2_on = bullet2_active && 
                        (hcount >= bullet2_x && hcount < bullet2_x + BULLET_SIZE &&
                        vcount >= bullet2_y && vcount < bullet2_y + BULLET_SIZE);

    // VGA output logic
    always_comb begin
        {VGA_R, VGA_G, VGA_B} = {8'h00, 8'h00, 8'h00};
        if (VGA_BLANK_n) begin
            if (ship1_on)
                {VGA_R, VGA_G, VGA_B} = {8'hFF, 8'h00, 8'h00};  // Red ship
            else if (ship2_on)
                {VGA_R, VGA_G, VGA_B} = {8'h00, 8'h00, 8'hFF};  // Blue ship
            else if (bullet1_on)
                {VGA_R, VGA_G, VGA_B} = {8'hFF, 8'hFF, 8'h00};  // Yellow bullet for ship 1
            else if (bullet2_on)
                {VGA_R, VGA_G, VGA_B} = {8'h00, 8'hFF, 8'hFF};  // Cyan bullet for ship 2
            else
                {VGA_R, VGA_G, VGA_B} = {background_r, background_g, background_b};
        end
    end

endmodule



// VGA timing generator module (unchanged from original)
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
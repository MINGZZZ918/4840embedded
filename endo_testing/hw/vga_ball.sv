/*
 * Avalon memory-mapped peripheral for VGA Ball Game (with ROM-based sprites)
 */

module vga_ball(
    input  logic        clk,
    input  logic        reset,

    // Avalon-MM write interface
    input  logic [7:0]  writedata,
    input  logic        write,
    input  logic        chipselect,
    input  logic [5:0]  address,

    // VGA outputs
    output logic [7:0]  VGA_R, VGA_G, VGA_B,
    output logic        VGA_CLK, VGA_HS, VGA_VS,
                        VGA_BLANK_n, VGA_SYNC_n
);

    // ----------------------------------------------------------------
    // Parameters
    // ----------------------------------------------------------------
    parameter MAX_BULLETS        = 5;
    parameter MAX_ENEMY_BULLETS  = 6;
    parameter IMAGE_WIDTH        = 64;
    parameter IMAGE_HEIGHT       = 64;
    parameter SHIP_WIDTH         = 16;
    parameter SHIP_HEIGHT        = 16;
    parameter BULLET_SIZE        = 4;
    parameter ENEMY_WIDTH        = 16;
    parameter ENEMY_HEIGHT       = 16;
    parameter ENEMY_BULLET_SIZE  = 4;

    // ----------------------------------------------------------------
    // Internal signals
    // ----------------------------------------------------------------
    // VGA scan counters
    logic [10:0]    hcount;
    logic [9:0]     vcount;

    // Background color
    logic [7:0]     background_r, background_g, background_b;

    // Ship position
    logic [10:0]    ship_x;
    logic [9:0]     ship_y;

    // Player bullets
    logic [10:0]    bullet_x[MAX_BULLETS];
    logic [9:0]     bullet_y[MAX_BULLETS];
    logic [MAX_BULLETS-1:0] bullet_active;

    // Enemies (2)
    logic [10:0]    enemy_x[2];
    logic [9:0]     enemy_y[2];
    logic [1:0]     enemy_active;

    // Enemy bullets
    logic [10:0]    enemy_bullet_x[MAX_ENEMY_BULLETS];
    logic [9:0]     enemy_bullet_y[MAX_ENEMY_BULLETS];
    logic [MAX_ENEMY_BULLETS-1:0] enemy_bullet_active;

    // Arbitrary image data (unchanged)
    logic [10:0]    image_x;
    logic [9:0]     image_y;
    logic           image_display;
    logic [7:0]     image_data[IMAGE_HEIGHT][IMAGE_WIDTH][3];

    // ROM-based sprites interface
    logic [11:0]    rom_address;
    logic [31:0]    rom_q;
    rom_sprites_altsyncram rom_sprites (
        .address_a(rom_address),
        .clock0(clk),
        .q_a(q_a)
    );

    // Derived pixel coordinates
    logic [10:0]    actual_hcount;
    logic [9:0]     actual_vcount;
    assign actual_hcount = {1'b0, hcount[10:1]};
    assign actual_vcount = vcount;

    // ----------------------------------------------------------------
    // Reset & Avalon-MM write logic
    // ----------------------------------------------------------------
    always_ff @(posedge clk) begin
        if (reset) begin
            // Background
            background_r <= 8'h00;
            background_g <= 8'h00;
            background_b <= 8'h20;

            // Ship default position
            ship_x <= 11'd200;
            ship_y <= 10'd240;

            // Clear bullets
            for (int i = 0; i < MAX_BULLETS; i++) begin
                bullet_x[i] <= 11'd0;
                bullet_y[i] <= 10'd0;
            end
            bullet_active <= '0;

            // Enemies
            enemy_x[0] <= 11'd800;
            enemy_x[1] <= 11'd800;
            enemy_y[0] <= 10'd150;
            enemy_y[1] <= 10'd350;
            enemy_active <= 2'b11;

            // Enemy bullets
            for (int i = 0; i < MAX_ENEMY_BULLETS; i++) begin
                enemy_bullet_x[i] <= 11'd0;
                enemy_bullet_y[i] <= 10'd0;
            end
            enemy_bullet_active <= '0;

            // Image data init
            image_x <= 11'd100;
            image_y <= 10'd100;
            image_display <= 1'b0;
            for (int i = 0; i < IMAGE_HEIGHT; i++)
                for (int j = 0; j < IMAGE_WIDTH; j++) begin
                    image_data[i][j][0] <= 8'd0; // R
                    image_data[i][j][1] <= 8'd0; // G
                    image_data[i][j][2] <= 8'd0; // B
                end
        end
        else if (chipselect && write) begin
            case (address)
                6'd0: background_r <= writedata;
                6'd1: background_g <= writedata;
                6'd2: background_b <= writedata;
                6'd3: ship_x[7:0] <= writedata;
                6'd4: ship_x[10:8] <= writedata[2:0];
                6'd5: ship_y[7:0] <= writedata;
                6'd6: ship_y[9:8] <= writedata[1:0];

                // Bullet flags
                6'd27: bullet_active <= writedata[MAX_BULLETS-1:0];

                // Enemy pos/status
                6'd28: enemy_x[0][7:0] <= writedata;
                6'd29: enemy_x[0][10:8] <= writedata[2:0];
                6'd30: enemy_y[0][7:0] <= writedata;
                6'd31: enemy_y[0][9:8] <= writedata[1:0];
                6'd32: enemy_x[1][7:0] <= writedata;
                6'd33: enemy_x[1][10:8] <= writedata[2:0];
                6'd34: enemy_y[1][7:0] <= writedata;
                6'd35: enemy_y[1][9:8] <= writedata[1:0];
                6'd36: enemy_active <= writedata[1:0];

                // Enemy bullet flags
                6'd61: enemy_bullet_active <= writedata[MAX_ENEMY_BULLETS-1:0];

                // Image x
                6'd62: image_x[7:0] <= writedata;
                6'd63: image_x[10:8] <= writedata[2:0];

                default: begin
                    // Player bullet positions
                    if (address >= 6'd7 && address < 6'd7 + 4*MAX_BULLETS) begin
                        int idx = (address - 6'd7) / 4;
                        int reg = (address - 6'd7) % 4;
                        case (reg)
                            0: bullet_x[idx][7:0] <= writedata;
                            1: bullet_x[idx][10:8] <= writedata[2:0];
                            2: bullet_y[idx][7:0] <= writedata;
                            3: bullet_y[idx][9:8] <= writedata[1:0];
                        endcase
                    end
                    // Enemy bullet positions
                    else if (address >= 6'd37 && address < 6'd37 + 4*MAX_ENEMY_BULLETS) begin
                        int eidx = (address - 6'd37) / 4;
                        int ereg = (address - 6'd37) % 4;
                        case (ereg)
                            0: enemy_bullet_x[eidx][7:0] <= writedata;
                            1: enemy_bullet_x[eidx][10:8] <= writedata[2:0];
                            2: enemy_bullet_y[eidx][7:0] <= writedata;
                            3: enemy_bullet_y[eidx][9:8] <= writedata[1:0];
                        endcase
                    end
                end
            endcase
        end
    end

    // ----------------------------------------------------------------
    // Sprite & image on-screen detection
    // ----------------------------------------------------------------
    // Ship display
    logic         ship_on;
    logic [3:0]   rel_x, rel_y;
    always_comb begin
        ship_on = 0;
        rel_x = 0; rel_y = 0;
        if (actual_hcount >= ship_x && actual_hcount < ship_x + SHIP_WIDTH &&
            actual_vcount >= ship_y && actual_vcount < ship_y + SHIP_HEIGHT) begin
            rel_x = actual_hcount - ship_x;
            rel_y = actual_vcount - ship_y;
            if (rel_x < SHIP_WIDTH && rel_y < SHIP_HEIGHT)
                ship_on = 1;
        end
    end

    // Enemy display
    logic         enemy_on;
    logic [3:0]   enemy_rel_x, enemy_rel_y;
    logic [0:0]   current_enemy;
    always_comb begin
        enemy_on = 0;
        enemy_rel_x = 0; enemy_rel_y = 0;
        current_enemy = 0;
        for (int i = 0; i < 2; i++) begin
            if (enemy_active[i] &&
                actual_hcount >= enemy_x[i] && actual_hcount < enemy_x[i] + ENEMY_WIDTH &&
                actual_vcount >= enemy_y[i] && actual_vcount < enemy_y[i] + ENEMY_HEIGHT) begin
                enemy_rel_x = actual_hcount - enemy_x[i];
                enemy_rel_y = actual_vcount - enemy_y[i];
                if (enemy_rel_x < ENEMY_WIDTH && enemy_rel_y < ENEMY_HEIGHT) begin
                    enemy_on = 1;
                    current_enemy = i;
                end
            end
        end
    end

    // Bullets (unchanged)
    logic bullet_on;
    always_comb begin
        bullet_on = 0;
        for (int i = 0; i < MAX_BULLETS; i++) begin
            if (bullet_active[i] &&
                actual_hcount >= bullet_x[i] && actual_hcount < bullet_x[i] + BULLET_SIZE &&
                actual_vcount >= bullet_y[i] && actual_vcount < bullet_y[i] + BULLET_SIZE &&
                bullet_x[i] < 11'd1280)
                bullet_on = 1;
        end
    end

    logic enemy_bullet_on;
    always_comb begin
        enemy_bullet_on = 0;
        for (int i = 0; i < MAX_ENEMY_BULLETS; i++) begin
            if (enemy_bullet_active[i] &&
                actual_hcount >= enemy_bullet_x[i] && actual_hcount < enemy_bullet_x[i] + ENEMY_BULLET_SIZE &&
                actual_vcount >= enemy_bullet_y[i] && actual_vcount < enemy_bullet_y[i] + ENEMY_BULLET_SIZE)
                enemy_bullet_on = 1;
        end
    end

    // Image display (unchanged)
    logic image_on;
    logic [7:0] image_pixel_r, image_pixel_g, image_pixel_b;
    logic [5:0] img_x, img_y;
    always_comb begin
        image_on = 0;
        img_x = 0; img_y = 0;
        image_pixel_r = 0; image_pixel_g = 0; image_pixel_b = 0;
        if (image_display &&
            actual_hcount >= image_x && actual_hcount < image_x + IMAGE_WIDTH &&
            actual_vcount >= image_y && actual_vcount < image_y + IMAGE_HEIGHT) begin
            img_x = actual_hcount - image_x;
            img_y = actual_vcount - image_y;
            if (img_x < IMAGE_WIDTH && img_y < IMAGE_HEIGHT) begin
                image_pixel_r = image_data[img_y][img_x][0];
                image_pixel_g = image_data[img_y][img_x][1];
                image_pixel_b = image_data[img_y][img_x][2];
                if (|{image_pixel_r, image_pixel_g, image_pixel_b})
                    image_on = 1;
            end
        end
    end

    // ----------------------------------------------------------------
    // Sprite ROM address & pixel extraction
    // ----------------------------------------------------------------
    logic [7:0] sprite_pixel_r, sprite_pixel_g, sprite_pixel_b;
    always_comb begin
        rom_address = 12'd0;
        sprite_pixel_r = 8'd0;
        sprite_pixel_g = 8'd0;
        sprite_pixel_b = 8'd0;

        // Ship uses sprite index 0
        if (ship_on) begin
            rom_address = rel_y * SHIP_WIDTH + rel_x;
            sprite_pixel_r = rom_q[23:16];
            sprite_pixel_g = rom_q[15:8];
            sprite_pixel_b = rom_q[7:0];
        end
        // Enemy uses sprite index = 1 + current_enemy
        else if (enemy_on) begin
            rom_address = 12'd256 + current_enemy * 12'd256 + enemy_rel_y * ENEMY_WIDTH + enemy_rel_x;
            sprite_pixel_r = rom_q[23:16];
            sprite_pixel_g = rom_q[15:8];
            sprite_pixel_b = rom_q[7:0];
        end
    end

    // ----------------------------------------------------------------
    // VGA output & layering
    // ----------------------------------------------------------------
    always_comb begin
        {VGA_R, VGA_G, VGA_B} = 24'h000000;
        if (VGA_BLANK_n) begin
            // Background
            {VGA_R, VGA_G, VGA_B} = {background_r, background_g, background_b};

            // Image
            if (image_on)
                {VGA_R, VGA_G, VGA_B} = {image_pixel_r, image_pixel_g, image_pixel_b};

            // Enemy sprite
            if (enemy_on)
                {VGA_R, VGA_G, VGA_B} = {sprite_pixel_r, sprite_pixel_g, sprite_pixel_b};

            // Ship sprite
            if (ship_on)
                {VGA_R, VGA_G, VGA_B} = {sprite_pixel_r, sprite_pixel_g, sprite_pixel_b};

            // Player bullet
            if (bullet_on)
                {VGA_R, VGA_G, VGA_B} = {8'hFF, 8'hFF, 8'h00};

            // Enemy bullet
            if (enemy_bullet_on)
                {VGA_R, VGA_G, VGA_B} = {8'hFF, 8'h40, 8'h00};
        end
    end

endmodule


// ------------------------------------------------------------------------
// VGA timing generator (unchanged)
// ------------------------------------------------------------------------
module vga_counters(
    input logic        clk50, reset,
    output logic [10:0] hcount,
    output logic [9:0]  vcount,
    output logic        VGA_CLK, VGA_HS, VGA_VS, VGA_BLANK_n, VGA_SYNC_n
);
    parameter HACTIVE      = 11'd1280,
              HFRONT_PORCH = 11'd32,
              HSYNC        = 11'd192,
              HBACK_PORCH  = 11'd96,
              HTOTAL       = HACTIVE + HFRONT_PORCH + HSYNC + HBACK_PORCH;
    parameter VACTIVE      = 10'd480,
              VFRONT_PORCH = 10'd10,
              VSYNC        = 10'd2,
              VBACK_PORCH  = 10'd33,
              VTOTAL       = VACTIVE + VFRONT_PORCH + VSYNC + VBACK_PORCH;

    logic endOfLine, endOfField;
    always_ff @(posedge clk50 or posedge reset)
        if (reset) hcount <= 0;
        else if (endOfLine) hcount <= 0;
        else hcount <= hcount + 1;
    assign endOfLine = (hcount == HTOTAL - 1);

    always_ff @(posedge clk50 or posedge reset)
        if (reset) vcount <= 0;
        else if (endOfLine) begin
            if (endOfField) vcount <= 0;
            else vcount <= vcount + 1;
        end
    assign endOfField = (vcount == VTOTAL - 1);

    assign VGA_HS     = !((hcount[10:8] == 3'b101) & !(hcount[7:5] == 3'b111));
    assign VGA_VS     = !(vcount[9:1] == (VACTIVE + VFRONT_PORCH) / 2);
    assign VGA_SYNC_n = 1'b0;
    assign VGA_BLANK_n= !(hcount[10] & (hcount[9] | hcount[8])) &
                        !(vcount[9] | (vcount[8:5] == 4'b1111));
    assign VGA_CLK    = hcount[0];
endmodule

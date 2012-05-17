---------------------------------------------------------------------------------------------------
-- Filename    : usbrx_toplevel.vhd
-- Project     : OsmoSDR FPGA Firmware
-- Purpose     : Toplevel component
---------------------------------------------------------------------------------------------------

-----------------------------------------------------------------------------------
-- Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany --
-- written by Matthias Kleffel                                                   --
--                                                                               --
-- This program is free software; you can redistribute it and/or modify          --
-- it under the terms of the GNU General Public License as published by          --
-- the Free Software Foundation as version 3 of the License, or                  --
--                                                                               --
-- This program is distributed in the hope that it will be useful,               --
-- but WITHOUT ANY WARRANTY; without even the implied warranty of                --
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  --
-- GNU General Public License V3 for more details.                               --
--                                                                               --
-- You should have received a copy of the GNU General Public License             --
-- along with this program. If not, see <http://www.gnu.org/licenses/>.          --
-----------------------------------------------------------------------------------

library ieee;
	use ieee.std_logic_1164.all;
	use ieee.std_logic_misc.all;
	use ieee.numeric_std.all;
library work;
	use work.all;
	use work.mt_toolbox.all;
	use work.usbrx.all;
	
entity usbrx_toplevel is
	port (
		-- common
		clk_in_pclk : in  std_logic;
		
		-- special control
		dings    : in  std_logic;
		dingsrst : in  std_logic;
		
		-- ADC interface
		adc_cs   : out std_logic;
		adc_sck  : out std_logic;
		adc_sd1  : in  std_logic;
		adc_sd2  : in  std_logic;
		
		-- control SPI
		ctl_int  : out std_logic;
		ctl_cs   : in  std_logic;
		ctl_sck  : in  std_logic;
		ctl_mosi : in  std_logic;
		ctl_miso : out std_logic;
	
		-- data SPIs
		rx_clk   : out std_logic;
		rx_syn   : out std_logic;
		rx_dat   : out std_logic;
		
		-- data SPIs
		tx_clk   : in  std_logic;
		tx_syn   : in  std_logic;
		tx_dat   : in  std_logic;
		
		-- gain PWMs
		gain0    : out std_logic;
		gain1    : out std_logic;
		
		-- GPS
		gps_1pps : in  std_logic;
		gps_10k  : in  std_logic;
	
		-- gpios
		gpio     : inout std_logic_vector(9 downto 0);
		
		-- virtual GNDs/VCCs
		vgnd     : out   std_logic_vector(11 downto 0);
		vcc33    : inout std_logic_vector(11 downto 0);
		vcc12    : inout std_logic_vector(4 downto 0)
	);
end usbrx_toplevel;

architecture rtl of usbrx_toplevel is

	-- clocks
	signal clk_30 : std_logic;
	signal clk_80 : std_logic;
	signal rst_30 : std_logic;
	signal rst_80 : std_logic;
	
	-- config
	signal cfg_pwm : usbrx_pwm_config_t;
	signal cfg_adc : usbrx_adc_config_t;
	signal cfg_ssc : usbrx_ssc_config_t;
	signal cfg_fil : usbrx_fil_config_t;
	signal cfg_off : usbrx_off_config_t;
	
	-- ADC <-> offset
	signal adc_off_clk : std_logic;
	signal adc_off_i   : unsigned(13 downto 0);
	signal adc_off_q   : unsigned(13 downto 0);
		
	-- offset <-> filter
	signal off_fil_clk : std_logic;
	signal off_fil_i   : signed(15 downto 0);
	signal off_fil_q   : signed(15 downto 0);
	
	-- filter <-> output
	signal fil_out_clk : std_logic;
	signal fil_out_i   : signed(15 downto 0);
	signal fil_out_q   : signed(15 downto 0);
	
	-- blinken lights
	signal blcnt1 : unsigned(31 downto 0) := x"00000000";
	signal blcnt2 : unsigned(31 downto 0) := x"00000000";
	signal blcnt3 : unsigned(31 downto 0) := x"00000000";
	signal blink1 : std_logic := '0';
	signal blink2 : std_logic := '0';
	signal blink3 : std_logic := '0';
	
	-- enusure that signal-names are kept (important for timing contraints)
	attribute syn_keep of clk_in_pclk : signal is true;
	
	-- debug
	signal dbg_ext  : std_logic;
	signal dbg_rst  : std_logic;
	signal dbg_lock : std_logic;
	
begin
	
	-- house-keeping
	cg: entity usbrx_clkgen
		port map (
			-- clock input
			clk_30_pclk	=> clk_in_pclk,
			ext_rst     => dingsrst,
			
			-- system clock
			clk_30 => clk_30,
			clk_80 => clk_80,
			rst_30 => rst_30,
			rst_80 => rst_80,
			
			-- debug
			dbg_ext  => dbg_ext,
			dbg_rst  => dbg_rst,
			dbg_lock => dbg_lock
		);
	
	-- register bank
	rb: entity usbrx_regbank
		port map (
			-- common
			clk   => clk_80,
			reset => rst_80,
			
			-- config
			cfg_pwm => cfg_pwm,
			cfg_adc => cfg_adc,
			cfg_ssc => cfg_ssc,
			cfg_fil => cfg_fil,
			cfg_off => cfg_off,
			
			-- status (TODO HACK)
			adc_i   => adc_off_i,
			adc_q   => adc_off_q,
		
			-- SPI interface
			spi_ncs  => ctl_cs,
			spi_sclk => ctl_sck,
			spi_mosi => ctl_mosi,
			spi_miso => ctl_miso
		);
	
	-- gain PWMs
	pwm: entity usbrx_pwm
		port map (
			-- common
			clk    => clk_80,
			reset  => rst_80,
	
			-- config
			config => cfg_pwm,
			
			-- PWM output
			pwm0   => gain0,
			pwm1   => gain1
		);
		
	-- A/D interface
	adc: entity usbrx_ad7357
		port map (
			-- common
			clk   => clk_80,
			reset => rst_80,
	
			-- config
			config => cfg_adc,
		
			-- ADC interface
			adc_cs  => adc_cs,
			adc_sck => adc_sck,
			adc_sd1 => adc_sd1,
			adc_sd2 => adc_sd2,
			
			-- output
			out_clk => adc_off_clk,
			out_i   => adc_off_i,
			out_q   => adc_off_q
		);
		
	-- offset stage
	off: entity usbrx_offset
		port map (
			-- common
			clk     => clk_80,
			reset   => rst_80,
			
			-- config
			config  => cfg_off,
			
			-- input
			in_clk  => adc_off_clk,
			in_i    => adc_off_i,
			in_q    => adc_off_q,
			
			-- output
			out_clk => off_fil_clk,
			out_i   => off_fil_i,
			out_q   => off_fil_q
		);


	-- decimation filter
 	fil: entity usbrx_decimate
		port map (
			-- common
			clk     => clk_80,
			reset   => rst_80,
			
			-- config
			config  => cfg_fil,
			
			-- input
			in_clk  => off_fil_clk,
			in_i    => off_fil_i,
			in_q    => off_fil_q,
			
			-- output
			out_clk => fil_out_clk,
			out_i   => fil_out_i,
			out_q   => fil_out_q
		);

	-- SSC output
	ssc: entity usbrx_ssc
		port map (
			-- common
			clk    => clk_80,
			reset  => rst_80,
			
			-- config 
			config => cfg_ssc,
			
			-- output
			in_clk => fil_out_clk,
			in_i   => fil_out_i,
			in_q   => fil_out_q,
			
			-- SSC interface
			ssc_clk => rx_clk,
			ssc_syn => rx_syn,
			ssc_dat => rx_dat
		);

	-- TODO
	ctl_int <= '1';
--	tx_clk  <= '0';
	
	-- blinken lights
	process(clk_in_pclk)
	begin
		if rising_edge(clk_in_pclk) then
			if blcnt1=0 then
				blcnt1 <= to_unsigned(30000000/2-1, 32);
				blink1 <= not blink1;
			else
				blcnt1 <= blcnt1-1;
			end if;
		end if;
	end process;
	process(clk_30)
	begin
		if rising_edge(clk_30) then
			if rst_30='1' then
				blcnt2 <= (others=>'0');
				blink2 <= '0';
			elsif blcnt2=0 then
				blcnt2 <= to_unsigned(30000000/2-1, 32);
				blink2 <= not blink2;
			else
				blcnt2 <= blcnt2-1;
			end if;
		end if;
	end process;
	process(clk_80)
	begin
		if rising_edge(clk_80) then
			if rst_80='1' then
				blcnt3 <= (others=>'0');
				blink3 <= '0';
			elsif blcnt3=0 then
				blcnt3 <= to_unsigned(80000000/2-1, 32);
				blink3 <= not blink3;
			else
				blcnt3 <= blcnt3-1;
			end if;
		end if;
	end process;
	
	--             8        7        6         5         4          3        2        1 
	gpio <= "00" & clk_80 & clk_30 & dbg_ext & dbg_rst & dbg_lock & blink3 & blink2 & blink1;
	
	-- virtual GNDs/VCCs
	vgnd  <= (others=>'0');
	vcc12 <= (others=>'Z');
	vcc33 <= (others=>'1');
		
end rtl;



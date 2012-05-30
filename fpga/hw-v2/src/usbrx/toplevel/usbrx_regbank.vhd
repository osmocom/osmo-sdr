---------------------------------------------------------------------------------------------------
-- Filename    : usbrx_regbank.vhd
-- Project     : OsmoSDR FPGA Firmware
-- Purpose     : Registerbank
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
	use ieee.numeric_std.all;
library work;
	use work.all;
	use work.mt_toolbox.all;
	use work.usbrx.all;

entity usbrx_regbank is
	port(
		-- common
		clk        : in  std_logic;
		reset      : in  std_logic;

		-- config
		cfg_pwm    : out usbrx_pwm_config_t;
		cfg_gpio   : out usbrx_gpio_config_t;
		cfg_adc    : out usbrx_adc_config_t;
		cfg_ssc    : out usbrx_ssc_config_t;
		cfg_fil    : out usbrx_fil_config_t;
		cfg_off    : out usbrx_off_config_t;
		
		-- status
		stat_ref   : in  usbrx_ref_status_t;
		stat_gpio  : in  usbrx_gpio_status_t;
	
		-- SPI interface
		spi_ncs    : in  std_logic;
		spi_sclk   : in  std_logic;
		spi_mosi   : in  std_logic;
		spi_miso   : out std_logic
	);
end usbrx_regbank;

architecture rtl of usbrx_regbank is

	-- bus interface
	signal bus_rena  : std_logic;
	signal bus_wena  : std_logic;
	signal bus_addr  : unsigned(6 downto 0);
	signal bus_rdata : std_logic_vector(31 downto 0);
	signal bus_wdata : std_logic_vector(31 downto 0);
		
	-- registers
	signal reg_pwm1 : std_logic_vector(31 downto 0);
	signal reg_pwm2 : std_logic_vector(31 downto 0);
	signal reg_adc  : std_logic_vector(15 downto 0);
	signal reg_ssc1 : std_logic_vector(0 downto 0);
	signal reg_ssc2 : std_logic_vector(7 downto 0);
	signal reg_fil  : std_logic_vector(2 downto 0);
	signal reg_off  : std_logic_vector(31 downto 0);
	signal reg_gain : std_logic_vector(31 downto 0);
	signal reg_swap : std_logic_vector(0 downto 0);
	signal reg_ioe  : std_logic_vector(10 downto 0);
	signal reg_iod  : std_logic_vector(10 downto 0);
	
	-- avoid block-ram inference
	attribute syn_romstyle : string;
	attribute syn_romstyle of rtl : architecture is "logic"; 
	
begin
	
	-- SPI slave
	spi: entity usbrx_spi
		port map (
			-- common
			clk       => clk,
			reset     => reset,
	
			-- SPI interface
			spi_ncs   => spi_ncs,
			spi_sclk  => spi_sclk,
			spi_mosi  => spi_mosi,
			spi_miso  => spi_miso,
			
			-- bus interface
			bus_rena  => bus_rena,
			bus_wena  => bus_wena,
			bus_addr  => bus_addr,
			bus_rdata => bus_rdata,
			bus_wdata => bus_wdata
		);

	-- handle requests
	process(reset, clk)
	begin
		if reset = '1' then
			bus_rdata <= (others=>'0');
			reg_pwm1  <= x"17701F3F";
			reg_pwm2  <= x"07D01F3F";
			reg_adc   <= x"0401";
			reg_ssc1  <= "0";
			reg_ssc2  <= x"01";
			reg_fil   <= "011";	
			reg_off   <= x"00000000";
			reg_gain  <= x"80008000";
			reg_swap  <= "0";
			reg_ioe   <= "00000000000";
			reg_iod   <= "00000000000";
		elsif rising_edge(clk) then
			-- output zeros by default
			bus_rdata <= (others=>'0');
			
			-- handle requests
			case to_integer(bus_addr) is
				when 0 =>
					-- identification
					bus_rdata <= x"DEADBEEF";
					
				when 1 =>
					-- PWM #1
					bus_rdata <= reg_pwm1;
					if bus_wena='1' then
						reg_pwm1 <= bus_wdata(31 downto 0);
					end if;
					
				when 2 =>
					-- PWM #2
					bus_rdata <= reg_pwm2;
					if bus_wena='1' then
						reg_pwm2 <= bus_wdata(31 downto 0);
					end if;
					
				when 3 =>
					-- ADC interface
					bus_rdata <= to_slv32(reg_adc);
					if bus_wena='1' then
						reg_adc <= bus_wdata(15 downto 0);
					end if;

				when 4 =>
					-- SSC interface
					bus_rdata( 0 downto 0) <= reg_ssc1;
					bus_rdata(15 downto 8) <= reg_ssc2;
					if bus_wena='1' then
						reg_ssc1 <= bus_wdata( 0 downto 0);
						reg_ssc2 <= bus_wdata(15 downto 8);
					end if;
			
				when 5 =>
					-- <unused>
					null;
					
				when 6 =>
					-- decimation filter
					bus_rdata <= to_slv32(reg_fil);
					if bus_wena='1' then
						reg_fil <= bus_wdata(2 downto 0);
					end if;
					
				when 7 =>
				 	-- sample offset
					bus_rdata <= reg_off;
					if bus_wena='1' then
						reg_off <= bus_wdata(31 downto 0);
					end if;
				when 8 =>
					-- sample gain
					bus_rdata <= reg_gain;
					if bus_wena='1' then
						reg_gain <= bus_wdata(31 downto 0);
					end if;
				when 9 =>
					-- sample swap
					bus_rdata(0 downto 0) <= reg_swap;
					if bus_wena='1' then
						reg_swap <= bus_wdata( 0 downto 0);
					end if;
					
				when 10 =>
					-- GPIO - output enable
					bus_rdata(10 downto 0) <= reg_ioe;
					if bus_wena='1' then
						reg_ioe <= bus_wdata(10 downto 0);
					end if;
				when 11 =>
					-- GPIO - output data
					bus_rdata(10 downto 0) <= reg_iod;
					if bus_wena='1' then
						reg_iod <= bus_wdata(10 downto 0);
					end if;
				when 12 =>
					-- GPIO - input data
					bus_rdata(10 downto 0) <= stat_gpio.idata;
					
				when 13 =>
					-- reference frequency
					bus_rdata(24 downto  0) <= std_logic_vector(stat_ref.lsb);
					bus_rdata(31 downto 25) <= std_logic_vector(stat_ref.msb);
					
				when others =>
					-- invalid address
					null;
			end case;
		end if;
	end process;
	
	-- map registers to config
	cfg_pwm.freq0  <= unsigned(reg_pwm1(15 downto  0));
	cfg_pwm.freq1  <= unsigned(reg_pwm2(15 downto  0));
	cfg_pwm.duty0  <= unsigned(reg_pwm1(31 downto 16));
	cfg_pwm.duty1  <= unsigned(reg_pwm2(31 downto 16));
	cfg_adc.clkdiv <= unsigned(reg_adc( 7 downto  0));
	cfg_adc.acqlen <= unsigned(reg_adc(15 downto  8));
	cfg_ssc.tmode  <= reg_ssc1(0);
	cfg_ssc.clkdiv <= unsigned(reg_ssc2);
	cfg_fil.decim  <= unsigned(reg_fil);
	cfg_fil.decim  <= unsigned(reg_fil);
	cfg_off.ioff   <= signed(reg_off(15 downto  0));
	cfg_off.qoff   <= signed(reg_off(31 downto 16));
	cfg_off.igain  <= unsigned(reg_gain(15 downto  0));
	cfg_off.qgain  <= unsigned(reg_gain(31 downto 16));
	cfg_off.swap   <= reg_swap(0);
	cfg_gpio.oena  <= reg_ioe;
	cfg_gpio.odata <= reg_iod;
	
end rtl;

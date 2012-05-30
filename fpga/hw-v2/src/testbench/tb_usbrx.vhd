---------------------------------------------------------------------------------------------------
-- Filename    : usbrx_toplevel.vhd
-- Project     : OsmoSDR FPGA Firmware Testbench
-- Purpose     : Toplevel Stimulus
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
	
entity tb_usbrx is
end tb_usbrx;

architecture rtl of tb_usbrx is

	-- common
	signal clk_in_pclk : std_logic := '1';
	
	-- special control
	signal dings    : std_logic;
	signal dingsrst : std_logic;
	
	-- ADC interface
	signal adc_cs   : std_logic;
	signal adc_sck  : std_logic;
	signal adc_sd1  : std_logic;
	signal adc_sd2  : std_logic;
	
	-- control SPI
	signal ctl_int  : std_logic;
	signal ctl_cs   : std_logic;
	signal ctl_sck  : std_logic;
	signal ctl_mosi : std_logic;
	signal ctl_miso : std_logic;

	-- data SPIs
	signal rx_clk   : std_logic;
	signal rx_syn   : std_logic;
	signal rx_dat   : std_logic;
	
	-- data SPIs
	signal tx_clk   : std_logic := '1';
	signal tx_syn   : std_logic;
	signal tx_dat   : std_logic;
	
	-- gain PWMs
	signal gain0    : std_logic;
	signal gain1    : std_logic;
	
	-- GPS
	signal gps_1pps : std_logic;
	signal gps_10k  : std_logic;

	-- gpios
	signal gpio     : std_logic_vector(9 downto 0);
	
	-- virtual GNDs/VCCs
	signal vgnd     : std_logic_vector(11 downto 0);
	signal vcc33    : std_logic_vector(11 downto 0);
	signal vcc12    : std_logic_vector(4 downto 0);
	
begin
	
	-- generate clocks
	clk_in_pclk <= not clk_in_pclk after 500 ns / 30.0;
	tx_clk <= '0'; --not tx_clk after 500 ns / 24.0;
	
	-- special control
	dings    <= '0';
	dingsrst <= '1'; --, '0' after 100 us, '1' after 200 us;
	
	-- data SPIs
	tx_syn <= '0';
	tx_dat <= '0';
	
	-- GPS
--	gps_1pps <= '0';
	gps_10k  <= '0';
	
	-- gpios
	gpio <= (others=>'H');
	
	-- generate pps signal
	-- (set every millisecond instead of every second
	--  to speed to simulation time)
	process
		variable cnt : natural;
	begin
		gps_1pps <= '0';
		
		cnt := 1;
		loop
			wait for (cnt * 1 ms) - now;
			gps_1pps <= '1';
			wait for 1us;
			gps_1pps <= '0';
			cnt := cnt+1;
		end loop;
		
		wait;
	end process;
	
	-- dummy ADC model
	process
--		constant word1 : unsigned(15 downto 0) := "0010000000000001";
--		constant word2 : unsigned(15 downto 0) := "0001111111111110";
--		constant word1 : unsigned(15 downto 0) := "0001111111111111";
--		constant word2 : unsigned(15 downto 0) := "0001111111111111";
		variable word1 : unsigned(15 downto 0) := "0011010111001101";
		variable word2 : unsigned(15 downto 0) := "0001001111000101";
		variable sreg1 : unsigned(15 downto 0);
		variable sreg2 : unsigned(15 downto 0);
		
		variable cnt : natural;
		
	begin
		adc_sd1 <= 'Z';
		adc_sd2 <= 'Z';
		
		cnt := 0;
		loop
			wait until falling_edge(adc_cs);
			
			word1 := to_unsigned(8192 + cnt, 16);
			word2 := to_unsigned(8192 - cnt, 16);
			cnt := (cnt + 1) mod 8192;
			
			sreg1 := word1;
			sreg2 := word2;
			
			adc_sd1 <= transport 'X', sreg1(15) after 5ns;
			adc_sd2 <= transport 'X', sreg2(15) after 5ns;
			sreg1 := shift_left(sreg1,1);
			sreg2 := shift_left(sreg2,1);
			
			il: loop
				wait until rising_edge(adc_cs) or falling_edge(adc_sck);
				exit when rising_edge(adc_cs); 
				
				adc_sd1 <= transport 'X', sreg1(15) after 11.0ns;
				adc_sd2 <= transport 'X', sreg2(15) after 11.0ns;
				sreg1 := shift_left(sreg1,1);
				sreg2 := shift_left(sreg2,1);
			end loop;
			
			adc_sd1 <= transport 'X', 'Z' after 9.5ns;
			adc_sd2 <= transport 'X', 'Z' after 9.5ns;
			
		end loop;
		
	end process;
	
	-- SPI interface
	process
	
		-- write cycle 
		procedure spi_write(addr: in integer; data: in slv32_t) is
			variable sreg : std_logic_vector(39 downto 0);
		begin
			-- assemble message
			sreg(39)           := '0';
			sreg(38 downto 32) := std_logic_vector(to_unsigned(addr,7));
			sreg(31 downto 0)  := data;
			
			-- assert CS
			ctl_sck  <= '1';
			ctl_mosi <= '1';
			ctl_cs   <= '0';
			wait for 250ns;
			
			-- clock out data
			for i in 39 downto 0 loop
				ctl_sck  <= '0';
				ctl_mosi <= sreg(i);
				wait for 250ns;
				ctl_sck  <= '1';
				wait for 250ns;
			end loop;
			
			-- deassert CS
			wait for 250ns;
			ctl_cs <= '1';
			wait for 250ns;
		end procedure spi_write;
		
		-- write cycle 
		procedure spi_writem(addr,count: in integer; data: in slv32_array_t) is
			variable sreg : std_logic_vector(31 downto 0);
		begin

			-- assert CS
			ctl_sck  <= '1';
			ctl_mosi <= '1';
			ctl_cs   <= '0';
			wait for 250ns;
			
			-- write command
			sreg(7)          := '0';
			sreg(6 downto 0) := std_logic_vector(to_unsigned(addr,7));
			for i in 7 downto 0 loop
				ctl_sck <= '0';
				ctl_mosi <= sreg(i);
				wait for 250ns;
				ctl_sck <= '1';
				wait for 250ns;
			end loop;
			
			--write data
			for j in 0 to count-1  loop
				sreg := data(j);
				for i in 31 downto 0 loop
					ctl_sck <= '0';
					ctl_mosi <= sreg(i);
					wait for 250ns;
					ctl_sck <= '1';
					wait for 250ns;
				end loop;
			end loop;
			
			-- deassert CS
			wait for 250ns;
			ctl_cs <= '1';
			wait for 250ns;
		end procedure spi_writem;
		
		-- read cycle 
		procedure spi_read(addr,count: in integer; data: out slv32_array_t) is
			variable sreg : std_logic_vector(7 downto 0);
		begin
			
			-- assemble message
			sreg(7)          := '1';
			sreg(6 downto 0) := std_logic_vector(to_unsigned(addr,7));
			
			-- assert CS
			ctl_sck  <= '1';
			ctl_mosi <= '1';
			ctl_cs   <= '0';
			wait for 250ns;
			
			-- clock out command
			for i in 7 downto 0 loop
				ctl_sck  <= '0';
				ctl_mosi <= sreg(i);
				wait for 250ns;
				ctl_sck  <= '1';
				wait for 250ns;
			end loop;
		
			wait for 50us;
			
			-- read data
			for j in 0 to count-1 loop
				for i in 31 downto 0 loop
					ctl_sck <= '0';
					wait for 250ns;
					data(j)(i)  := ctl_miso;
					ctl_sck <= '1';
					wait for 250ns;
					
					if i=24 or i=16 or i=8 then
						wait for 50us;
					end if;
					
				end loop;
			end loop;
				
			-- deassert CS
			wait for 250ns;
			ctl_cs <= '1';
			wait for 250ns;
		end procedure spi_read;
		
		variable temp : slv32_array_t(0 to 5);
		
	begin
		ctl_cs   <= '1';
		ctl_sck  <= '1';
		ctl_mosi <= '1';
		
		wait for 30us;
		
--		spi_write(4,x"00000001");
		
--		spi_read(0,1,temp);
--		spi_read(0,1,temp);
--		wait;
--		
--		temp(0) := x"00000000";
--		temp(1) := x"12345678";
--		temp(2) := x"9ABCDEF0";
--		temp(3) := x"11233435";
--		temp(4) := x"23652662";
--		temp(5) := x"98735773";
--		spi_writem(0,6,temp);
--
--		temp := (others=>x"00000000");
--		spi_read(0,6,temp);
		
		wait;
	end process;
	
	-- unit under test
	uut: entity usbrx_toplevel
		port map (
			-- common
			clk_in_pclk => clk_in_pclk,
			
			-- special control
			dings    => dings,
			dingsrst => dingsrst,
			
			-- ADC interface
			adc_cs   => adc_cs,
			adc_sck  => adc_sck,
			adc_sd1  => adc_sd1,
			adc_sd2  => adc_sd2,
			
			-- control SPI
			ctl_int  => ctl_int,
			ctl_cs   => ctl_cs,
			ctl_sck  => ctl_sck,
			ctl_mosi => ctl_mosi,
			ctl_miso => ctl_miso,
		
			-- data SPIs
			rx_clk   => rx_clk,
			rx_syn   => rx_syn,
			rx_dat   => rx_dat,
			
			-- data SPIs
			tx_clk   => tx_clk,
			tx_syn   => tx_syn,
			tx_dat   => tx_dat,
			
			-- gain PWMs
			gain0    => gain0,
			gain1    => gain1,
			
			-- GPS
			gps_1pps => gps_1pps,
			gps_10k  => gps_10k,
			
			-- gpios
			gpio     => gpio,
			
			-- virtual GNDs/VCCs
			vgnd     => vgnd,
			vcc33    => vcc33,
			vcc12    => vcc12
		);
	
end rtl;



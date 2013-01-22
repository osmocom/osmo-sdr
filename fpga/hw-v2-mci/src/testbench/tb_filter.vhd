---------------------------------------------------------------------------------------------------
-- Filename    : usbrx_toplevel.vhd
-- Project     : OsmoSDR FPGA Firmware Testbench
-- Purpose     : Decimation Filter Stimulus
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
	use ieee.math_real.all;
library work;
	use work.all;
	use work.mt_toolbox.all;
	use work.mt_filter.all;
	use work.usbrx.all;
	
entity tb_filter is
end tb_filter;

architecture rtl of tb_filter is

	-- common
	signal clk      : std_logic := '1';
	signal reset    : std_logic := '1';

	-- config
	signal config   : usbrx_fil_config_t;
		
	-- input
	signal in_clk   : std_logic;
	signal in_i	    : signed(15 downto 0);
	signal in_q   	: signed(15 downto 0);
	
	-- output
	signal out_clk  : std_logic;
	signal out_i	: signed(15 downto 0);
	signal out_q	: signed(15 downto 0);

begin
	
	-- generate clock
	clk   <= not clk after 500ns / 100.0;
	reset <= '1', '0' after 123ns;
	
	-- set config
	config.decim <= "110";
	
	-- input control
	process
		variable t : real := 0.0;
		variable f : real := 1.0;
	begin
		in_clk  <= '0';
		in_i    <= to_signed(0,16);
		in_q    <= to_signed(0,16);
		wait until rising_edge(clk) and reset='0';
		
		loop
			-- wait some time
			for i in 0 to 38 loop
				wait until rising_edge(clk);
			end loop;
			
			-- get sample data
			in_i <= to_signed(integer(cos(t)*10000.0),16);
			in_q <= to_signed(0,16);

			-- input sample
			in_clk <= '1';
			wait until rising_edge(clk);
			in_clk <= '0';
			
			-- update time
			t := t + f/2000000.0*2.0*MATH_PI;
			if t >= 2.0*MATH_PI then
				t := t - 2.0*MATH_PI;
			end if;
			
			-- update frequency
			f := f + 1.0;
			if f>1000000.0 then
				f := 1.0;
			end if;
		end loop;
		
		wait;
	end process;
	
	-- create filter core
	uut: entity usbrx_decimate
		port map (
			-- common
			clk     => clk,
			reset   => reset,
			
			-- config
			config  => config,
			
			-- input
			in_clk  => in_clk,
			in_i    => in_i,
			in_q    => in_q,
			
			-- output
			out_clk => out_clk,
			out_i   => out_i,
			out_q   => out_q
		);
	
end rtl;



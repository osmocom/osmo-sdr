---------------------------------------------------------------------------------------------------
-- Filename    : mt_fil_mac_slow.vhd
-- Project     : maintech filter toolbox
-- Purpose     : MAC cell for FIR-like filters
--               - version for 'slow' filter versions
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

-------------------------------------------------------------------------------
-- mt_fil_mac_slow ------------------------------------------------------------
-------------------------------------------------------------------------------

library ieee;
	use ieee.std_logic_1164.all;
	use ieee.numeric_std.all;
library work;
	use work.all;
	use work.mt_toolbox.all;
	use work.mt_filter.all;

entity mt_fil_mac_slow is
	port (
		-- common
		clk 	: in  std_logic;
		reset 	: in  std_logic;
		
		-- control-path
		start   : in  std_logic;
		active  : in  std_logic;
		presub  : in  std_logic;
		
		-- data input
		smp1	: in  fir_dataword18;
		smp2	: in  fir_dataword18;
		coeff	: in  fir_dataword18;
		
		-- data output
		dnew	: out std_logic;
		dout	: out fir_dataword18
	);
end mt_fil_mac_slow;

architecture rtl of mt_fil_mac_slow is

	-- rounding constant (16 bits will get truncated)
	constant RNDVAL : natural := (2**16/2);

	-- control signals
	signal done       : std_logic;
	signal active_del : std_logic_vector(2 downto 0);
	signal start_del  : std_logic_vector(2 downto 0);
	signal done_del   : std_logic_vector(2 downto 0);

	-- data registers
	signal psreg : std_logic;
	signal dreg  : signed(17 downto 0);
	signal b0reg : signed(17 downto 0);
	signal b1reg : signed(18 downto 0);
	signal a0reg : signed(17 downto 0);
	signal a1reg : signed(17 downto 0);
	signal mreg  : signed(35 downto 0);
	signal preg  : signed(35 downto 0);
	
begin
	
	-- create done-flag after 'active' goes low or 'start' is set while still active
	done <= (start or (not active)) and active_del(0);
	
	-- create delayed control-signals
	process(clk)
	begin
		if rising_edge(clk) then
			active_del <= active_del(active_del'left-1 downto 0) & active;
			start_del  <= start_del(start_del'left-1 downto 0) & start;
			done_del   <= done_del(done_del'left-1 downto 0) & done;
		end if;
	end process;
	
	-- do math
	process(clk)
	begin
		if rising_edge(clk) then
			-- simple storage registers
			psreg <= presub;
			dreg  <= smp1;
			b0reg <= smp2;
			a0reg <= coeff;
			a1reg <= a0reg;
			
			-- pre-adder
			if psreg='1'
				then b1reg <= resize(b0reg,19) - resize(dreg,19);
				else b1reg <= resize(b0reg,19) + resize(dreg,19);
			end if;
			
			-- multiplier
			mreg <= a1reg * b1reg(18 downto 1);
			
			-- post-adder / accumulator
			if active_del(2)='1' then
				if start_del(2)='1' 
					then preg <= mreg + to_signed(RNDVAL,36);
					else preg <= mreg + preg;
				end if;
			end if;
		end if;
	end process;
		
	-- update output
	process(reset, clk)
	begin
		if reset='1' then
			dnew <= '0';
			dout <= (others=>'0');
		elsif rising_edge(clk) then
			if done_del(2)='1' then
				dnew <= '1';
				if preg(35)='0' and preg(34 downto 33)/="00" then
					dout <= to_signed(2**17-1,18);
				elsif preg(35)='1' and preg(34 downto 33)/="11" then
					dout <= to_signed(-(2**17),18);
				else
					dout <= preg(33 downto 16);
				end if;
			else
				dnew <= '0';
			end if;
		end if;
	end process;
end rtl;

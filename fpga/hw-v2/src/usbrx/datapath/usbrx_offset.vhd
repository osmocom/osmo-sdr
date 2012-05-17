---------------------------------------------------------------------------------------------------
-- Filename    : usbrx_halfband.vhd
-- Project     : OsmoSDR FPGA Firmware
-- Purpose     : Programmable sample value offset
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

entity usbrx_offset is
	port (
		-- common
		clk     : in  std_logic;
		reset   : in  std_logic;
		
		-- config
		config  : in  usbrx_off_config_t;
		
		-- input
		in_clk  : in  std_logic;
		in_i    : in  unsigned(13 downto 0);
		in_q    : in  unsigned(13 downto 0);
		
		-- output
		out_clk : out std_logic;
		out_i   : out signed(15 downto 0);
		out_q   : out signed(15 downto 0)
	);
end usbrx_offset;

architecture rtl of usbrx_offset is		

	-- clip & saturate sample
	function doClipValue(x : signed) return signed is
	begin
		if x >= 32768 then
			-- overflow
			return to_signed(+32767,16);
		elsif x < -32768 then
			-- underflow
			return to_signed(-32768,16);
		else
			-- in range
			return x(15 downto 0);
		end if;
	end doClipValue;
	
begin
	
	-- control logic
	process(clk)
		variable s16i, s16q : signed(15 downto 0);
		variable s17i, s17q : signed(16 downto 0);
	begin
		if rising_edge(clk) then
			-- passthough clock
			out_clk <= in_clk;
			
			-- handle data
			if in_clk='1' then
				-- convert input into 16bit signed
				s16i := signed(in_i xor "10000000000000") & "00";
				s16q := signed(in_q xor "10000000000000") & "00";
				
				-- add offset
				s17i := resize(s16i,17) + resize(config.ioff,17);
				s17q := resize(s16q,17) + resize(config.qoff,17);
				
				-- clip output
				out_i <= doClipValue(s17i);
				out_q <= doClipValue(s17q);
			end if;
			
			-- handle reset
			if reset='1' then
				out_clk <= '0';
				out_i   <= (others=>'0');
				out_q   <= (others=>'0');
			end if;
		end if;
	end process;
	
end rtl;

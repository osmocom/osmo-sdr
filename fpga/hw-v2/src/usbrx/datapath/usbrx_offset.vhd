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
		variable xnorm : signed(x'length-1 downto 0) := x;
	begin
		if xnorm >= 32768 then
			-- overflow
			return to_signed(+32767,16);
		elsif xnorm < -32768 then
			-- underflow
			return to_signed(-32768,16);
		else
			-- in range
			return xnorm(15 downto 0);
		end if;
	end doClipValue;
	
	-- multiplier input
	signal mula_i, mula_q : signed(17 downto 0) := (others=>'0');
	signal mulb_i, mulb_q : signed(17 downto 0) := (others=>'0');
	
	-- multiplier output
	signal mout_i, mout_q : signed(18 downto 0) := (others=>'0');
	
begin
	
	-- control logic
	process(clk)
		variable mtmp_i, mtmp_q : signed(35 downto 0);
		variable atmp_i, atmp_q : signed(19 downto 0);
	begin
		if rising_edge(clk) then
			-- passthough clock
			out_clk <= in_clk;
			
			-- handle data
			if in_clk='1' then
				-- apply swap-flag & convert input into 18bit signed
				if config.swap='0' then
					mula_i <= signed(in_i xor "10000000000000") & "0000";
					mula_q <= signed(in_q xor "10000000000000") & "0000";
				else
					mula_i <= signed(in_q xor "10000000000000") & "0000";
					mula_q <= signed(in_i xor "10000000000000") & "0000";
				end if;
				
				-- apply gain
				mulb_i <= signed("00" & config.igain);
				mulb_q <= signed("00" & config.qgain);
				mtmp_i := mula_i * mulb_i;
				mtmp_q := mula_q * mulb_q;
				mout_i <= mtmp_i(34 downto 16);
				mout_q <= mtmp_q(34 downto 16);
				
				-- add offset (also adds 0.5 for rounding of multiplier-output)
				atmp_i := resize(mout_i,20) + resize(config.ioff&"1",20);
				atmp_q := resize(mout_q,20) + resize(config.qoff&"1",20);
				
				-- clip output
				out_i <= doClipValue(atmp_i(19 downto 1));
				out_q <= doClipValue(atmp_q(19 downto 1));
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

---------------------------------------------------------------------------------------------------
-- Filename    : mt_toolbox.vhd
-- Project     : maintech IP-Core toolbox
-- Purpose     : maintech toolbox package
--
-- Description : declaration of common types, functions and attributes
--               used throughout the toolbox
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
	
package mt_toolbox is
	
	--
	-- basic types
	--
	subtype slv8_t  is std_logic_vector(7 downto 0);
	subtype slv16_t is std_logic_vector(15 downto 0);
	subtype slv32_t is std_logic_vector(31 downto 0);
	subtype byte_t  is unsigned(7 downto 0);
	subtype word_t  is unsigned(15 downto 0);
	subtype dword_t is unsigned(31 downto 0);
	type slv8_array_t  is array (natural range<>) of slv8_t;
	type slv16_array_t is array (natural range<>) of slv16_t;
	type slv32_array_t is array (natural range<>) of slv32_t;
	type byte_array_t  is array (natural range<>) of byte_t;
	type word_array_t  is array (natural range<>) of word_t;
	type dword_array_t is array (natural range<>) of dword_t;

	--
	-- simple helper functions
	--
	function log2(x: natural) return positive;
	
	--
	-- type conversion helper
	--
	function to_slv8(x: std_logic) return std_logic_vector;
	function to_slv8(x: std_logic_vector) return std_logic_vector;
	function to_slv8(x: unsigned) return std_logic_vector;
	function to_slv8(x: signed) return std_logic_vector;
	function to_slv8(x: natural) return std_logic_vector;
	function to_slv16(x: std_logic) return std_logic_vector;
	function to_slv16(x: std_logic_vector) return std_logic_vector;
	function to_slv16(x: unsigned) return std_logic_vector;
	function to_slv16(x: signed) return std_logic_vector;
	function to_slv16(x: natural) return std_logic_vector;
	function to_slv32(x: std_logic) return std_logic_vector;
	function to_slv32(x: std_logic_vector) return std_logic_vector;
	function to_slv32(x: unsigned) return std_logic_vector;
	function to_slv32(x: signed) return std_logic_vector;
	function to_slv32(x: natural) return std_logic_vector;
	
	--
	-- common attributes
	--
	attribute syn_keep      : boolean;
	attribute syn_ramstyle  : string;
	attribute syn_romstyle  : string;
	attribute shreg_extract : string;
	
end mt_toolbox;

package body mt_toolbox is
	
	--
	-- simple helper functions
	--

	-- calculate ceiling base 2 logarithm (returns always >=1)
	function log2(x: natural) return positive is
		variable x_tmp: natural;
		variable y: positive;
	begin
		x_tmp := x-1;
		y := 1;
		while x_tmp > 1 loop
			y := y+1;
			x_tmp := x_tmp/2;
		end loop;
		return y;
	end;

	-- to_slv8 (pack basic types into "std_logic_vector(7 downto 0)")
	function to_slv8(x: std_logic) return std_logic_vector is
		variable res : std_logic_vector(7 downto 0);
	begin
		res := (0=>x,others=>'0');
		return res;
	end to_slv8;
	function to_slv8(x: std_logic_vector) return std_logic_vector is
		variable res : std_logic_vector(7 downto 0);
	begin
		res := (others=>'0');
		res(x'length-1 downto 0) := x;
		return res;
	end to_slv8;
	function to_slv8(x: unsigned) return std_logic_vector is
	begin
		return to_slv8(std_logic_vector(x));
	end to_slv8;
	function to_slv8(x: signed) return std_logic_vector is
	begin
		return to_slv8(std_logic_vector(x));
	end to_slv8;
	function to_slv8(x: natural) return std_logic_vector is
	begin
		return to_slv8(to_unsigned(x,8));
	end to_slv8;
	
	-- to_slv16 (pack basic types into "std_logic_vector(15 downto 0)")
	function to_slv16(x: std_logic) return std_logic_vector is
		variable res : std_logic_vector(15 downto 0);
	begin
		res := (0=>x,others=>'0');
		return res;
	end to_slv16;
	function to_slv16(x: std_logic_vector) return std_logic_vector is
		variable res : std_logic_vector(15 downto 0);
	begin
		res := (others=>'0');
		res(x'length-1 downto 0) := x;
		return res;
	end to_slv16;
	function to_slv16(x: unsigned) return std_logic_vector is
	begin
		return to_slv16(std_logic_vector(x));
	end to_slv16;
	function to_slv16(x: signed) return std_logic_vector is
	begin
		return to_slv16(std_logic_vector(x));
	end to_slv16;
	function to_slv16(x: natural) return std_logic_vector is
	begin
		return to_slv16(to_unsigned(x,16));
	end to_slv16;
	
	-- to_slv32 (pack basic types into "std_logic_vector(31 downto 0)")
	function to_slv32(x: std_logic) return std_logic_vector is
		variable res : std_logic_vector(31 downto 0);
	begin
		res := (0=>x,others=>'0');
		return res;
	end to_slv32;
	function to_slv32(x: std_logic_vector) return std_logic_vector is
		variable res : std_logic_vector(31 downto 0);
	begin
		res := (others=>'0');
		res(x'length-1 downto 0) := x;
		return res;
	end to_slv32;
	function to_slv32(x: unsigned) return std_logic_vector is
	begin
		return to_slv32(std_logic_vector(x));
	end to_slv32;
	function to_slv32(x: signed) return std_logic_vector is
	begin
		return to_slv32(std_logic_vector(x));
	end to_slv32;
	function to_slv32(x: natural) return std_logic_vector is
	begin
		return to_slv32(to_unsigned(x,32));
	end to_slv32;
end mt_toolbox;


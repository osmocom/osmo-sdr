---------------------------------------------------------------------------------------------------
-- Filename    : mt_filter.vhd
-- Project     : maintech filter toolbox
-- Purpose     : maintech filter toolbox package
--
-- Description : declaration of common types, functions and attributes
--               used throughout the filter toolbox
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
	
package mt_filter is

	--
	-- FIR filter types
	--
	subtype fir_dataword18   is signed(17 downto 0); 
	type    fir_databus18    is array (natural range <>) of fir_dataword18; 
	subtype fir_coefficient  is integer range -2**17 to 2**17-1; 	
	type    fir_coefficients is array (natural range <>) of fir_coefficient; 	
	
end mt_filter;

package body mt_filter is
	-- nothing so far
end mt_filter;


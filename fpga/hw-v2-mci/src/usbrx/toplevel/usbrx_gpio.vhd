---------------------------------------------------------------------------------------------------
-- Filename    : usbrx_gpio.vhd
-- Project     : OsmoSDR FPGA Firmware
-- Purpose     : GPIO Block
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

entity usbrx_gpio is
	port(
		-- common
		clk    : in  std_logic;
		reset  : in  std_logic;
		
		-- GPIOs
		gpio   : inout std_logic_vector(10 downto 0);
		
		-- config / status
		config : in  usbrx_gpio_config_t;
		status : out usbrx_gpio_status_t
	);
end usbrx_gpio;

architecture rtl of usbrx_gpio is

	-- register
	signal ireg : std_logic_vector(10 downto 0) := (others=>'0');
	signal oreg : std_logic_vector(10 downto 0) := (others=>'0');
	signal oena : std_logic_vector(10 downto 0) := (others=>'0');
	
begin
	
	-- create output-driver
	od: for i in gpio'range generate
	begin
		gpio(i) <= oreg(i) when oena(i)='1' else 'Z';
	end generate;
	
	-- IOBs
	process(clk)
	begin
		if rising_edge(clk) then
			ireg <= to_X01(gpio);
			oreg <= config.odata;
			oena <= config.oena;
		end if;
	end process;
	
end rtl;

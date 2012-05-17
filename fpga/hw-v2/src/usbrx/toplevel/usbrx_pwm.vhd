---------------------------------------------------------------------------------------------------
-- Filename    : usbrx_pwm.vhd
-- Project     : OsmoSDR FPGA Firmware
-- Purpose     : PWM Generator
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

entity usbrx_pwm is
	port(
		-- common
		clk       : in  std_logic;
		reset     : in  std_logic;

		-- config
		config    : in  usbrx_pwm_config_t;
		
		-- PWM output
		pwm0      : out std_logic;
		pwm1      : out std_logic
	);
end usbrx_pwm;

architecture rtl of usbrx_pwm is

	-- status
	signal counter0 : unsigned(15 downto 0);
	signal counter1 : unsigned(15 downto 0);
	signal out0     : std_logic;
	signal out1     : std_logic;
	
begin
	
	process(clk)
	begin
		if rising_edge(clk) then
			-- update counter #0
			counter0 <= counter0 - 1;
			if counter0 = 0 then
				counter0 <= config.freq0;
			end if;
			
			-- update counter #1
			counter1 <= counter1 - 1;
			if counter1 = 0 then
				counter1 <= config.freq1;
			end if;
			
			-- update output #0
			if counter0 < config.duty0
				then out0 <= '1';
				else out0 <= '0';
			end if;
			
			-- update output #1
			if counter1 < config.duty1
				then out1 <= '1';
				else out1 <= '0';
			end if;
			
			-- output register
			pwm0 <= out0;
			pwm1 <= out1;
				
			-- handle reset
			if reset='1' then
				counter0 <= (others=>'0');
				counter1 <= (others=>'0');
				out0     <= '0';
				out1     <= '0';
				pwm0     <= '0';
				pwm1     <= '0';
			end if;
		end if;
	end process;
	
end rtl;

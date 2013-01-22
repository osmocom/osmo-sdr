---------------------------------------------------------------------------------------------------
-- Filename    : usbrx.vhd
-- Project     : OsmoSDR FPGA Firmware
-- Purpose     : OsmoSDR package
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
	
package usbrx is
	
	-- PWM config
	type usbrx_pwm_config_t is record
		freq0 : unsigned(15 downto 0);
		freq1 : unsigned(15 downto 0);
		duty0 : unsigned(15 downto 0);
		duty1 : unsigned(15 downto 0);
	end record;
		
	-- ADC interface config
	type usbrx_adc_config_t is record
		clkdiv : unsigned(7 downto 0);
		acqlen : unsigned(7 downto 0);
	end record;
		
	-- SSC interface config
	type usbrx_ssc_config_t is record
		clkdiv : unsigned(7 downto 0);
		tmode  : std_logic;
	end record;
	
	-- offset stage config
	type usbrx_off_config_t is record
		swap  : std_logic;
		ioff  : signed(15 downto 0);
		qoff  : signed(15 downto 0);
		igain : unsigned(15 downto 0);
		qgain : unsigned(15 downto 0);
	end record;
		
	-- decimation filter config
	type usbrx_fil_config_t is record
		decim : unsigned(2 downto 0);
	end record;
	
	-- clock reference status
	type usbrx_ref_status_t is record
		lsb : unsigned(24 downto 0);
		msb : unsigned(6 downto 0);
	end record;
		
	-- GPIO config
	type usbrx_gpio_config_t is record
		oena  : std_logic_vector(10 downto 0);
		odata : std_logic_vector(10 downto 0);
	end record;
	
	-- clock reference status
	type usbrx_gpio_status_t is record
		idata : std_logic_vector(10 downto 0);
	end record;
	
end usbrx;

package body usbrx is
	-- nothing so far
end usbrx;


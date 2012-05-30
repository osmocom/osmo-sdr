-----------------------------------------------------------------------------------
-- Filename    : mt_synctools.vhd
-- Project     : maintech IP-Core toolbox
-- Purpose     : Basic tools for clock-domain-crossings
--
-----------------------------------------------------------------------------------

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


------------------------------------------------------------------------------
-- mt_sync_dualff (dual flip-flop synchronizer) -------------------------------
-------------------------------------------------------------------------------

library ieee;
	use ieee.std_logic_1164.all;
library work;
	use work.mt_toolbox.all;

entity mt_sync_dualff is
	port(
		-- input
		i_data	: in  std_logic;
		
		-- output
		o_clk	: in  std_logic;
		o_data	: out std_logic
	);
end mt_sync_dualff;

architecture rtl of mt_sync_dualff is
	-- signals
	signal sreg	: std_logic := '0';
	signal oreg	: std_logic := '0';
	
	-- no SRL16s here...
	attribute shreg_extract of sreg : signal is "no";
	attribute shreg_extract of oreg : signal is "no";
	
begin	
	process(o_clk)
	begin
		if rising_edge(o_clk) then
			sreg <= i_data;
			oreg <= sreg;
		end if;
	end process;
	o_data <= oreg;
end rtl;


-------------------------------------------------------------------------------
-- mt_sync_feedback (feedback synchronizer) -----------------------------------
-------------------------------------------------------------------------------

library ieee;
	use ieee.std_logic_1164.all;
library work;
	use work.mt_toolbox.all;

entity mt_sync_feedback is
	port(
		-- input
		i_clk	: in  std_logic;
		i_data	: in  std_logic;
		
		-- output
		o_clk	: in  std_logic;
		o_data	: out std_logic
	);
end mt_sync_feedback;

architecture rtl of mt_sync_feedback is

	signal flip_i  : std_logic := '0';
	signal flip_s1 : std_logic := '0';
	signal flip_s2 : std_logic := '0';
	signal flip_s3 : std_logic := '0';
	signal oreg    : std_logic := '0';

	attribute syn_keep of flip_i  : signal is true;
	attribute syn_keep of flip_s1 : signal is true;
	attribute syn_keep of flip_s2 : signal is true;
	attribute syn_keep of flip_s3 : signal is true;
	attribute syn_keep of oreg    : signal is true;
	
begin
	
	process(i_clk)
	begin
		if rising_edge(i_clk) then
			-- update flip-bit on request
			if i_data='1' then
				flip_i <= not flip_i;
			end if;
			
			-- debug check
			assert not (i_data='1' and flip_s1/=flip_i)
				report "mt_sync_feedback: pulses too close, failed to synchronize"
				severity failure;
		end if;
	end process;
	
	process(o_clk)
	begin
		if rising_edge(o_clk) then
			-- synchronize flip-bit
			flip_s1 <= flip_i;
			flip_s2 <= flip_s1;
			flip_s3 <= flip_s2;
			
			-- create output-request
			oreg <= flip_s3 xor flip_s2; 
		end if;
	end process;

	-- set output
	o_data <= oreg;
	
end rtl;

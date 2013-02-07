
-----------------------------------------------------------------------------------
-- Copyright (C) 2013 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany --
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
library work;
	use work.mt_toolbox.all;
	use work.all;
	
entity sdio_txctrl is
	port(
		-- clocks
		clk_sys   : in  std_logic;
		rst_sys   : in  std_logic;
		sd_clk    : in  std_logic;
		
		-- config
		cfg_bsize : in  unsigned(15 downto 0);
		
		-- input
		in_clk    : in  std_logic;
		in_data   : in  std_logic_vector(31 downto 0);
		
		-- output
		out_ready : out std_logic;
		out_sync  : in  std_logic;
		out_clk   : in  std_logic;
		out_dat   : out std_logic_vector(7 downto 0)
	);
end sdio_txctrl;

architecture rtl of sdio_txctrl is


	-- FIFO output
	signal fifo_fill  : unsigned(10 downto 0);
	signal fifo_oclk  : std_logic;
	signal fifo_odata : std_logic_vector(31 downto 0);
	
	-- output control
	signal ophase : unsigned(1 downto 0);
	signal obuf   : std_logic_vector(23 downto 0);
			
begin
	
	-- control logic
	process(sd_clk,rst_sys)
	begin
		if rst_sys='1' then
			fifo_oclk <= '0';
			out_ready <= '0';
			out_dat   <= (others=>'0');
			ophase    <= (others=>'0');
			obuf      <= (others=>'0');
		elsif rising_edge(sd_clk) then
			-- set default values
			fifo_oclk <= '0';
			
			-- create output-redy-flag
			if fifo_fill >= (cfg_bsize/4)
				then out_ready <= '1';
				else out_ready <= '0';
			end if;
			
			-- handle output-request
			if out_clk='1' then
				if out_sync='1' or ophase=0 then
					out_dat   <= fifo_odata( 7 downto 0);
					obuf      <= fifo_odata(31 downto 8);
					ophase    <= "11";
					fifo_oclk <= '1';
				else
					out_dat   <= obuf( 7 downto 0);
					obuf      <= x"00" & obuf(23 downto 8);
					ophase    <= ophase-1;
				end if;
			end if;
		end if;
	end process;
	
	-- big data fifo
	fifo: entity sdio_txfifo
		port map (
			-- system clock
			clk_sys  => clk_sys,
			rst_sys  => rst_sys,
			
			-- SDC-clock
			clk_sdc  => sd_clk,
			rst_sdc  => rst_sys,
			
			-- input
			in_clk   => in_clk,
			in_data  => in_data,
			
			-- output ('clk_sdc')
			out_fill => fifo_fill,
			out_clk  => fifo_oclk,
			out_data => fifo_odata
		);
	
end rtl;

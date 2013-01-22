---------------------------------------------------------------------------------------------------
-- Filename    : usbrx_sdio.vhd
-- Project     : OsmoSDR FPGA Firmware
-- Purpose     : ATSAM3U SDIO Interface
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

entity usbrx_sdio is
	port(
		-- common
		clk     : in  std_logic;
		reset   : in  std_logic;

		-- config
		config  : in  usbrx_ssc_config_t;
		
		-- input
		in_clk  : in  std_logic;
		in_i    : in  signed(15 downto 0);
		in_q    : in  signed(15 downto 0);
		
		-- SDIO interface
		sd_clk  : in    std_logic;
		sd_cmd  : inout std_logic;
		sd_dat  : inout std_logic_vector(3 downto 0)
	);
end usbrx_sdio;

architecture rtl of usbrx_sdio is

	-- helper function
	function to_signed(x : unsigned) return signed is
	begin
		return signed((not x(x'left)) & x(x'left-1 downto 0));
	end to_signed;
	function pack(i,q : unsigned) return slv32_t is
		variable res : slv32_t := (others=>'0');
	begin
		res(31 downto 32-i'length) := std_logic_vector(to_signed(i));
		res(15 downto 16-q'length) := std_logic_vector(to_signed(q));
		return res;
	end pack;
	function pack(i,q : signed) return slv32_t is
		variable res : slv32_t := (others=>'0');
	begin
		res(31 downto 32-i'length) := std_logic_vector(i);
		res(15 downto 16-q'length) := std_logic_vector(q);
		return res;
	end pack;
	
	-- SDIO input
	signal sdio_clk : std_logic;
	signal sdio_dat : std_logic_vector(31 downto 0);
	
	-- debug
	signal counter1 : unsigned(15 downto 0);
	signal counter2 : unsigned(15 downto 0);
	
begin
	
	-- control logic
	process(clk)
	begin
		if rising_edge(clk) then
			-- set default values
			sdio_clk <= '0';
			
			-- handle incoming samples
			if in_clk='1' then
				-- latch sample
				sdio_dat <= pack(in_i,in_q);
				sdio_clk <= '1';
				
				-- apply test-mode
				if config.tmode='1' then
					sdio_dat <= pack(counter1,counter2);
					counter1 <= counter1 + 1;
					counter2 <= counter2 - 1;
				end if;
			end if;
				
			-- handle reset
			if reset='1' then
				sdio_clk <= '0';
				sdio_dat <= (others=>'0');
				counter1 <= (others=>'0');
				counter2 <= (others=>'1');
			end if;
		end if;
	end process;

	-- SDIO controller core
	sdio: entity sdio_top
		port map (
			-- common
			clk     => clk,
			reset   => reset,
			
			-- input
			in_clk  => sdio_clk,
			in_data => sdio_dat,
			
			-- SDIO interface
			sd_clk  => sd_clk,
			sd_cmd  => sd_cmd,
			sd_dat  => sd_dat
		);
	
end rtl;

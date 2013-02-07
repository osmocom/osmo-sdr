---------------------------------------------------------------------------------------------------
-- Filename    : usbrx_spi.vhd
-- Project     : OsmoSDR FPGA Firmware
-- Purpose     : SPI Slave Implementation
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

entity usbrx_spi is
	port(
		-- common
		clk       : in  std_logic;
		reset     : in  std_logic;

		-- SPI interface
		spi_ncs   : in  std_logic;
		spi_sclk  : in  std_logic;
		spi_mosi  : in  std_logic;
		spi_miso  : out std_logic;
		
		-- bus interface
		bus_rena  : out std_logic;
		bus_wena  : out std_logic;
		bus_addr  : out unsigned(6 downto 0);
		bus_rdata : in  std_logic_vector(31 downto 0);
		bus_wdata : out std_logic_vector(31 downto 0)
	);
end usbrx_spi;

architecture rtl of usbrx_spi is

	-- internal types
	type state_t is (S_COMMAND, S_WRITE, S_READ2, S_READ3, S_READ4);
	
	-- IO-registers
	signal iob_ncs  : std_logic;
	signal iob_sclk : std_logic;
	signal iob_mosi : std_logic;

	-- synchronized inputs
	signal sync_ncs  : std_logic;
	signal sync_sclk : std_logic;
	signal sync_mosi : std_logic;
	
	-- edge detection
	signal last_sclk : std_logic;
	signal last_re   : std_logic;
	signal last_fe   : std_logic;
	
	-- SPI slave
	signal state  : state_t;
	signal remain : unsigned(5 downto 0);
	signal sireg  : std_logic_vector(31 downto 0);
	signal soreg  : std_logic_vector(32 downto 0);
	signal addr   : unsigned(6 downto 0);

begin
	
	-- IOBs
	process(clk)
	begin
		if rising_edge(clk) then
			iob_ncs  <= spi_ncs;
			iob_sclk <= spi_sclk;
			iob_mosi <= spi_mosi;
			if iob_ncs='0' 
				then spi_miso <= soreg(32);
				else spi_miso <= '1';
			end if;
		end if;
	end process;
	
	-- input synchronizer
	process(clk,reset)
	begin
		if reset = '1' then
			sync_ncs  <= '1';
			sync_sclk <= '0';
			sync_mosi <= '0';
		elsif rising_edge(clk) then
			sync_ncs  <= iob_ncs;
			sync_sclk <= iob_sclk;
			sync_mosi <= iob_mosi;
		end if;
	end process;

	-- SPI slave
	process(clk,reset)
		variable re,fe : std_logic;
	begin
		if reset = '1' then
			last_sclk <= '0';
			last_re   <= '0';
			last_fe   <= '0';
			state     <= S_COMMAND;
			remain    <= (others=>'0');
			sireg     <= (others=>'1');
			soreg     <= (others=>'1');
			addr      <= (others=>'0');
			bus_rena  <= '0';
			bus_wena  <= '0';
			bus_addr  <= (others=>'0');
			bus_wdata <= (others=>'0');
		elsif rising_edge(clk) then
			-- set default values
			bus_rena  <= '0';
			bus_wena <= '0';
	
			-- detect edges on clock line
			last_sclk <= sync_sclk;
			re := sync_sclk and (not last_sclk);
			fe := (not sync_sclk) and last_sclk;
			last_re <= re;
			last_fe <= fe;
		
			-- update shift-registers
			if re='1' then
				sireg <= sireg(30 downto 0) & sync_mosi;
			end if;
			if fe='1' then
				soreg <= soreg(31 downto 0) & '1';
			end if;
	
			-- check state of chip-select
			if sync_ncs='1' then
				--> CS deasserted, reset slave
				state  <= S_COMMAND;
				remain <= to_unsigned(7,6);
			else
				--> CS asserted, tick state-machine
				case state is
					when S_COMMAND =>
						-- wait until 8 bits were received
						if last_re='1' then
							remain <= remain - 1;
							if remain=0 then
								--> got 8 bits, decode address & direction
								addr   <= unsigned(sireg(6 downto 0));
								remain <= to_unsigned(31,6);
								if sireg(7)='1' then
									state    <= S_READ2;
									bus_rena <= '1';
									bus_addr <= unsigned(sireg(6 downto 0));
								else 
									state <= S_WRITE;
								end if;
							end if;
						end if;
						
					when S_WRITE =>
						-- wait until 32 bits were received
						if last_re='1' then
							remain <= remain - 1;
							if remain=0 then
								-- issue write-request
								bus_wena  <= '1';
								bus_addr  <= addr;
								bus_wdata <= sireg;
								
								-- continue with next word
								addr   <= addr + 1;
								remain <= to_unsigned(31,6);
							end if;
						end if;
					
					when S_READ2 =>
						-- wait-state
						state  <= S_READ3;
						
					when S_READ3 =>
						-- load shift-register
						soreg  <= soreg(32) & bus_rdata;
						state  <= S_READ4;
					    remain <= to_unsigned(31,6);
						
					when S_READ4 =>
						-- wait until 32 bits were transmitted
						if fe='1' then
							remain <= remain - 1;
							if remain=0 then
								-- continue with next word
								bus_rena <= '1';
								bus_addr <= addr + 1;
								addr     <= addr + 1;
								state    <= S_READ2;
							end if;
						end if;
				end case;
			end if;
		end if;
	end process;
	
end rtl;

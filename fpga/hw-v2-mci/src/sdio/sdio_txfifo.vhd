
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
	
entity sdio_txfifo is
	port(
		-- system clock
		clk_sys  : in  std_logic;
		rst_sys  : in  std_logic;
		
		-- SDC-clock
		clk_sdc  : in  std_logic;
		rst_sdc  : in  std_logic;
		
		-- input ('clk_sys')
		in_clk   : in  std_logic;
		in_data  : in  slv32_t;
		
		-- output ('clk_sdc')
		out_fill : out unsigned(10 downto 0);
		out_clk  : in  std_logic;
		out_data : out slv32_t
	);
end sdio_txfifo;

architecture rtl of sdio_txfifo is	

	-- fifo pointers
	signal wptr    : unsigned(10 downto 0) := (others=>'0');
	signal wptr_s1 : unsigned(10 downto 0) := (others=>'0');
	signal wptr_s2 : unsigned(10 downto 0) := (others=>'0');
	signal rptr    : unsigned(10 downto 0) := (others=>'0');
	signal rptr_s1 : unsigned(10 downto 0) := (others=>'0');
	signal rptr_s2 : unsigned(10 downto 0) := (others=>'0');
	
	-- create fifo
	type memory_t is array (0 to 2047) of slv32_t;
	signal mem : memory_t;

begin
	
	-- input control
	process(clk_sys, rst_sys)
		variable fill : unsigned(10 downto 0);
	begin
		if rst_sys='1' then
			rptr_s1 <= (others=>'0');
			rptr_s2 <= (others=>'0');
			wptr    <= (others=>'0');
		elsif rising_edge(clk_sys) then
			-- synchronize read-pointer
			rptr_s1 <= rptr;
			rptr_s2 <= rptr_s1;
			
			-- get current fill-count
			fill := gray2bin(wptr) - gray2bin(rptr_s2);
			
			-- wait for incoming data
			if in_clk='1' then
				-- store entry in info
				mem(to_integer(wptr)) <= in_data;
				
				-- update write-pointer
				if fill/=2047 then
					--> increment pointer
					wptr <= bin2gray(gray2bin(wptr)+1);
				else
					--> overflow
					report "sdio_txfifo: fifo overflow"
						severity error;
				end if;
			end if;
		end if;
	end process;
	
	-- output control
	process(clk_sdc, rst_sdc)
		variable fill : unsigned(10 downto 0);
	begin
		if rst_sdc='1' then
			wptr_s1  <= (others=>'0');
			wptr_s2  <= (others=>'0');
			rptr     <= (others=>'0');
			out_fill <= (others=>'0');
--			out_data <= (others=>'0');
		elsif rising_edge(clk_sdc) then
			-- synchronize write-pointer
			wptr_s1 <= wptr;
			wptr_s2 <= wptr_s1;
			
			-- get current fill-count
			fill := gray2bin(wptr_s2) - gray2bin(rptr);
			out_fill <= fill;
			
			-- read entry from info
			out_data <= mem(to_integer(rptr));
			
			-- update read-pointer
			if out_clk='1' then
				if fill/=0 then
					--> increment pointer
					rptr <= bin2gray(gray2bin(rptr)+1);
				else
					--> overflow
					report "sdio_txfifo: fifo underflow"
						severity error;
				end if;
			end if;
		end if;
	end process;
	
				
end rtl;

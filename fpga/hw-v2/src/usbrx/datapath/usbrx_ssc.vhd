---------------------------------------------------------------------------------------------------
-- Filename    : usbrx_ssc.vhd
-- Project     : OsmoSDR FPGA Firmware
-- Purpose     : ATSAM3U SSC Interface
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

entity usbrx_ssc is
	port(
		-- common
		clk     : in  std_logic;
		reset   : in  std_logic;

		-- config
		config  : in  usbrx_ssc_config_t;
		
		-- output
		in_clk  : in  std_logic;
		in_i    : in  signed(15 downto 0);
		in_q    : in  signed(15 downto 0);
		
		-- SSC interface
		ssc_clk : out std_logic;
		ssc_syn : out std_logic;
		ssc_dat : out std_logic
	);
end usbrx_ssc;

architecture rtl of usbrx_ssc is

	-- CLK generator
	signal cg_div   : unsigned(7 downto 0);
	signal cg_phase : std_logic;
	signal cg_tick  : std_logic;
	
	-- shift register
	signal sreg   : std_logic_vector(31 downto 0);
	signal remain : unsigned(5 downto 0);
	signal nxtsyn : std_logic;

	-- input latch
	signal lreg   : std_logic_vector(31 downto 0);
	signal filled : std_logic;
	
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
	
	-- debug
	signal counter1 : unsigned(15 downto 0);
	signal counter2 : unsigned(15 downto 0);
	
begin
	
	-- clock generator 
	process(clk)
	begin
		if rising_edge(clk) then
			-- set default values
			cg_tick <= '0';
			
			-- update clock-divider
			if cg_div=0 then
				-- toggle phase
				cg_phase <= not cg_phase;
				cg_div   <= config.clkdiv;
				
				-- set 'tick'-flag when generating falling edge
				if cg_phase='1' then
					cg_tick <= '1';
				end if;
			else
				-- stay in current phase
				cg_div <= cg_div-1;
			end if;
			
			-- update output
			ssc_clk <= cg_phase;
			
			-- handle reset
			if reset='1' then
				cg_div   <= (others=>'0');
				cg_phase <= '0';
				cg_tick  <= '0';
				ssc_clk  <= '0';
			end if;
		end if;
	end process;
	
	-- output shift register
	process(clk)
	begin
		if rising_edge(clk) then
			-- wait for output-clock
			if cg_tick='1' then
				-- update output
				ssc_dat <= sreg(sreg'left);
				ssc_syn <= nxtsyn;
				sreg    <= sreg(sreg'left-1 downto 0) & '0';
				nxtsyn  <= '0';
				
				-- consume bit
				if remain > 0 then
					remain <= remain - 1;
				end if;
				
				-- reload shift-register when possible
				if filled='1' and remain<=1 then
					filled <= '0';
					remain <= to_unsigned(32, remain'length);
					nxtsyn <= '1';
					sreg   <= lreg;
				end if;
			end if;

			-- handle incoming samples
			if in_clk='1' then
				if filled='0' then
					-- latch sample
					lreg   <= pack(in_i,in_q);
					filled <= '1';
					
					-- apply test-mode
					if config.tmode='1' then
						lreg <= pack(counter1,counter2);
						counter1 <= counter1 + 1;
						counter2 <= counter2 - 1;
					end if;
				else
					--> overflow
					report "usbrx_ssc: input too fast"
						severity warning;
				end if;
			end if;
				
			-- handle reset
			if reset='1' then
				sreg    <= (others=>'0');
				remain  <= (others=>'0');
				nxtsyn  <= '0';
				lreg    <= (others=>'0');
				filled  <= '0';
				ssc_syn <= '0';
				ssc_dat <= '0';
				counter1 <= (others=>'0');
				counter2 <= (others=>'1');
			end if;
		end if;
	end process;
	
end rtl;

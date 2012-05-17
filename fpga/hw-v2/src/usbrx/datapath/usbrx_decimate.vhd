---------------------------------------------------------------------------------------------------
-- Filename    : usbrx_decimate.vhd
-- Project     : OsmoSDR FPGA Firmware
-- Purpose     : Variable decimation filter
--               (possible factors: 1,2,4,8,16,32,64)
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

-------------------------------------------------------------------------------
-- decimation filter ----------------------------------------------------------
-------------------------------------------------------------------------------

library ieee;
	use ieee.std_logic_1164.all;
	use ieee.numeric_std.all;
library work;
	use work.all;
	use work.mt_toolbox.all;
	use work.mt_filter.all;
	use work.usbrx.all;

entity usbrx_decimate is
	port (
		-- common
		clk     : in  std_logic;
		reset   : in  std_logic;
		
		-- config
		config  : in  usbrx_fil_config_t;
		
		-- input
		in_clk  : in  std_logic;
		in_i    : in  signed(15 downto 0);
		in_q    : in  signed(15 downto 0);
		
		-- output
		out_clk : out std_logic;
		out_i   : out signed(15 downto 0);
		out_q   : out signed(15 downto 0)
	);
end usbrx_decimate;

architecture rtl of usbrx_decimate is		

	-- config
	signal active : std_logic_vector(5 downto 0);

	-- adapted input
	signal in_si : fir_dataword18;
	signal in_sq : fir_dataword18;
	
	-- filter input
	signal fil_in_clk  : std_logic_vector(5 downto 0);
	signal fil_in_i	   : fir_databus18(5 downto 0);
	signal fil_in_q    : fir_databus18(5 downto 0);
	
	-- filter output
	signal fil_out_clk : std_logic_vector(5 downto 0);
	signal fil_out_i   : fir_databus18(5 downto 0);
	signal fil_out_q   : fir_databus18(5 downto 0);
	
	-- unclipped output
	signal nxt_clk : std_logic;
	signal nxt_i   : fir_dataword18;
	signal nxt_q   : fir_dataword18;
	
begin
	
	-- convert input into 18bit signed
	in_si <= signed(in_i) & "00";
	in_sq <= signed(in_q) & "00";
	
	-- control logic
	process(clk)
		variable tmp_i,tmp_q : fir_dataword18;
	begin
		if rising_edge(clk) then
			
			-- get active stages
			case to_integer(config.decim) is
				when 0      => active <= "000000";
				when 1      => active <= "000001";
				when 2      => active <= "000011";
				when 3      => active <= "000111";
				when 4      => active <= "001111";
				when 5      => active <= "011111";
				when others => active <= "111111";
			end case;
			
			-- select output
			case to_integer(config.decim) is
				when 0 => 
					nxt_clk <= in_clk;
					nxt_i   <= in_si;
					nxt_q   <= in_sq;
				when 1 => 
					nxt_clk <= fil_out_clk(0);
					nxt_i   <= fil_out_i(0);
					nxt_q   <= fil_out_q(0);
				when 2 => 
					nxt_clk <= fil_out_clk(1);
					nxt_i   <= fil_out_i(1);
					nxt_q   <= fil_out_q(1);
				when 3 => 
					nxt_clk <= fil_out_clk(2);
					nxt_i   <= fil_out_i(2);
					nxt_q   <= fil_out_q(2);
				when 4 => 
					nxt_clk <= fil_out_clk(3);
					nxt_i   <= fil_out_i(3);
					nxt_q   <= fil_out_q(3);
				when 5 => 
					nxt_clk <= fil_out_clk(4);
					nxt_i   <= fil_out_i(4);
					nxt_q   <= fil_out_q(4);
				when others => 
					nxt_clk <= fil_out_clk(5);
					nxt_i   <= fil_out_i(5);
					nxt_q   <= fil_out_q(5);
			end case;
			
			-- set output
			out_clk <= nxt_clk;
			if nxt_clk='1' then
				tmp_i := nxt_i + 2;
				tmp_q := nxt_q + 2;
				out_i <= tmp_i(17 downto 2);
				out_q <= tmp_q(17 downto 2);
			end if;
			
			-- handle reset
			if reset='1' then
				active  <= (others=>'0');
				nxt_clk <= '0';
				nxt_i   <= (others=>'0');
				nxt_q   <= (others=>'0');
				out_clk <= '0';
				out_i   <= (others=>'0');
				out_q   <= (others=>'0');
			end if;
		end if;
	end process;
	
	
	process(in_clk,in_si,in_sq,fil_out_clk,fil_out_i,fil_out_q,active)
	begin
		-- feed first filter stage
		fil_in_clk(0) <= in_clk and active(0);
		fil_in_i(0) <= in_si;
		fil_in_q(0) <= in_sq;
	
		-- chain remaining filter stages
		for i in 1 to 5 loop
			fil_in_clk(i) <= fil_out_clk(i-1) and active(i-1);
			fil_in_i(i) <= fil_out_i(i-1);
			fil_in_q(i) <= fil_out_q(i-1);
		end loop;
	end process;
	
	-- filter instance #1 (stage 0)
	hbf1: entity usbrx_halfband
		generic map (
			N => 1
		)
		port map (
			-- common
			clk     => clk,
			reset   => reset,
			
			-- input
			in_clk  => fil_in_clk(0 downto 0),
			in_i    => fil_in_i(0 downto 0),
			in_q    => fil_in_q(0 downto 0),
			
			-- output
			out_clk => fil_out_clk(0 downto 0),
			out_i   => fil_out_i(0 downto 0),
			out_q   => fil_out_q(0 downto 0)
		);

	-- filter instance #2 (stage 1-5)
	hbf2: entity usbrx_halfband
		generic map (
			N => 5
		)
		port map (
			-- common
			clk     => clk,
			reset   => reset,
			
			-- input
			in_clk  => fil_in_clk(5 downto 1),
			in_i    => fil_in_i(5 downto 1),
			in_q    => fil_in_q(5 downto 1),
			
			-- output
			out_clk => fil_out_clk(5 downto 1),
			out_i   => fil_out_i(5 downto 1),
			out_q   => fil_out_q(5 downto 1)
		);
		
end rtl;

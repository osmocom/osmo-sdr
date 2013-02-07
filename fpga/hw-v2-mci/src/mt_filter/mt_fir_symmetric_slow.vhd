---------------------------------------------------------------------------------------------------
-- Filename    : mt_fir_symmetric_slow.vhd
-- Project     : maintech filter toolbox
-- Purpose     : Symmetric FIR filter
--               - multiplexed input/output for all data-channels
--               - single MAC-cell for all calculations
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
	use work.mt_filter.all;

entity mt_fir_symmetric_slow is
	generic (
		CHANNELS   : natural;				-- number of data channels
		TAPS       : natural;				-- number of filter taps (AFTER folding)
		COEFFS     : fir_coefficients;		-- coefficent sets
		RAMSTYLE   : string;				-- ram style for inferred memories
		ROMSTYLE   : string					-- ram style for coefficent rom
	);
	port(
		-- common
		clk        : in  std_logic;
		reset      : in  std_logic;
		
		-- input port
		in_clk     : in  std_logic;
		in_ack     : out std_logic;
		in_chan    : in  unsigned(log2(CHANNELS)-1 downto 0);
		in_data    : in  fir_dataword18;
		
		-- output port
		out_clk    : out std_logic;
		out_chan   : out unsigned(log2(CHANNELS)-1 downto 0);
		out_data   : out fir_dataword18
	);
end mt_fir_symmetric_slow;

architecture rtl of mt_fir_symmetric_slow is	

	-- internal types
	subtype chan_t is unsigned(log2(CHANNELS)-1 downto 0);
	type chan_array_t is array(natural range<>) of chan_t;

	-- control signals
	signal active   : std_logic;
	signal shiftcnt : unsigned(log2(TAPS)-1 downto 0);
	signal ochan    : chan_array_t(3 downto 0);
	
	-- storage ports
	signal st_chan   : chan_t;
	signal st_start  : std_logic;
	signal st_stop   : std_logic;
	signal st_active : std_logic;
	signal st_din    : fir_dataword18;
	
	-- storage <-> MAC
	signal st_mac_start  : std_logic;
	signal st_mac_stop   : std_logic;
	signal st_mac_active : std_logic;
	signal st_mac_dout1  : fir_dataword18;
	signal st_mac_dout2  : fir_dataword18;
	signal st_mac_coeff  : fir_dataword18;
	
	-- MAC output
	signal mac_dnew : std_logic;
	signal mac_dout : fir_dataword18;
	
begin
	
	-- control logic
	process(clk, reset)
	begin
		if reset='1' then
			active     <= '0';
			shiftcnt   <= (others=>'0');
			ochan      <= (others=>(others=>'0'));
			st_start   <= '0';
			st_stop    <= '0';
			st_active  <= '0';
			in_ack     <= '0';
			out_clk    <= '0';
			out_data   <= (others=>'0');
			out_chan   <= (others=>'0');
		elsif rising_edge(clk) then
			-- set default values
			in_ack    <= '0';
			out_clk   <= '0';
			st_start  <= '0';
			st_stop   <= '0';
			st_active <= '0';
			
			-- get current status
			if active='0' then
				--> idle
				
				-- check for new request
				if in_clk='1' then
					--> input new sample and start burst from storage to MAC cell
					shiftcnt  <= to_unsigned(TAPS-1, shiftcnt'length);
					st_start  <= '1';
					st_active <= '1';
					active    <= '1';
				end if;
			else
				--> active
				
				-- control storage
				if shiftcnt/=0 then
					--> continue with burst
					shiftcnt  <= shiftcnt-1;
					st_active <= '1';
					if shiftcnt=1 then
						-- last cycle of burst
						st_stop <= '1';
						in_ack  <= '1'; 
						active  <= '0';
					end if;
				end if;
			end if;
			
			-- check if new result is ready
			if mac_dnew='1' then
				--> MAC done, update output
				out_clk  <= '1';
				out_chan <= ochan(ochan'left);
				out_data <= mac_dout;
			end if;
			
			-- delay channel-number to compensate for MAC delay
			ochan <= ochan(ochan'left-1 downto 0) & ochan(0);
			if st_mac_start='1' then
				ochan(0) <= in_chan;
			end if;
		end if;
	end process;
	
	-- connect storage input
	st_chan <= in_chan;
	st_din	<= in_data;
	
	-- data storage
	st: entity mt_fil_storage_slow
		generic map (
			COEFFS     => COEFFS,
			DCHAN      => CHANNELS,
			TAPS       => TAPS,
			RAMSTYLE   => RAMSTYLE,
			ROMSTYLE   => ROMSTYLE
		)
		port map (
			-- common
			clk        => clk,
			reset 	   => reset,
			
			-- config
			chan       => st_chan,
			
			-- input
			in_load	   => st_start,
			in_start   => st_start,
			in_stop    => st_stop,
			in_active  => st_active,
			in_data	   => st_din,
			
			-- output
			out_load   => open,
			out_start  => st_mac_start,
			out_stop   => st_mac_stop,
			out_active => st_mac_active,
			out_data1  => st_mac_dout1,
			out_data2  => st_mac_dout2,
			out_coeff  => st_mac_coeff
		);
		
	-- do create MAC cell
	mac: entity mt_fil_mac_slow
		port map (
			-- common
			clk    => clk,
			reset  => reset,
			
			-- control-path
			start  => st_mac_start,
			active => st_mac_active,
			presub => '0',
			
			-- data input
			smp1   => st_mac_dout1,
			smp2   => st_mac_dout2,
			coeff  => st_mac_coeff,
			
			-- data output
			dnew   => mac_dnew,
			dout   => mac_dout
		);
	
end rtl;

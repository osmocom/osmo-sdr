
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
	use ieee.numeric_std.all;
library work;
	use work.all;
	use work.mt_toolbox.all;

entity sdio_dat_tx is
	port(
		-- common
		clk       : in  std_logic;
		reset     : in  std_logic;
		
		-- control
		start     : in  std_logic;
		abort     : in  std_logic;
		bcount    : in  unsigned(8 downto 0);
		bsize     : in  unsigned(15 downto 0);
		
		-- status
		active    : out std_logic;
			
		-- input data
		in_ready  : in  std_logic;
		in_sync   : out std_logic;
		in_clk    : out std_logic;
		in_dat    : in  std_logic_vector(7 downto 0);
		
		-- SDIO interface
		sd_clk    : in  std_logic;
		sd_dat_oe : out std_logic;
		sd_dat_o  : out std_logic_vector(3 downto 0)
	);
	
	attribute syn_useioff : boolean;
	attribute syn_useioff of sd_dat_oe : signal is true; 
	attribute syn_useioff of sd_dat_o  : signal is true; 

end sdio_dat_tx;

architecture rtl of sdio_dat_tx is

	-- state
	type state_t is (
		S_INACTIVE,
		S_IDLE,
		S_START,
		S_DATA_H,
		S_DATA_L,
		S_CRC,
		S_STOP
	);
	
	-- status
	signal state   : state_t;
	signal bremain : unsigned(8 downto 0);
	signal remain  : unsigned(11 downto 0);
	signal lsb     : std_logic_vector(3 downto 0);
	
	-- CRCs
	signal crc0 : std_logic_vector(15 downto 0);
	signal crc1 : std_logic_vector(15 downto 0);
	signal crc2 : std_logic_vector(15 downto 0);
	signal crc3 : std_logic_vector(15 downto 0);
	
	-- update CRC16
	function crc16_update(crc : std_logic_vector; dat : std_logic) return std_logic_vector is
	begin
		if dat /= crc(15)
			then return (crc(14 downto 0) & "0") xor x"1021";
			else return (crc(14 downto 0) & "0");
		end if;
	end crc16_update;

begin
	
	-- control logic
	process(sd_clk,reset)
	begin
		if reset='1' then
			state     <= S_INACTIVE;
			bremain   <= (others=>'0');
			remain    <= (others=>'0');
			lsb       <= (others=>'0');
			crc0      <= (others=>'0');
			crc1      <= (others=>'0');
			crc2      <= (others=>'0');
			crc3      <= (others=>'0');
			in_clk    <= '0';
			in_sync   <= '0';
			sd_dat_oe <= '0';
			sd_dat_o  <= "0000";
		elsif rising_edge(sd_clk) then
			-- set default values;
			in_clk    <= '0';
			in_sync   <= '0';
			sd_dat_oe <= '0';
	
			-- update state
			case state is
				when S_INACTIVE =>
					-- wait for start-request
					if start='1' then
						bremain <= bcount;
						state <= S_IDLE;
					end if;
					
				when S_IDLE =>
					-- wait until input is ready
					if in_ready='1' then
						-- request first byte
						in_clk  <= '1';
						in_sync <= '1';
						
						-- update status
						remain <= resize(bsize-1,12);
						state  <= S_START;
					end if;
					
				when S_START =>
					-- output start-bits
					sd_dat_oe <= '1';
					sd_dat_o  <= "0000";
					
					-- update status
					state <= S_DATA_H;
					
				when S_DATA_H =>
					-- output MSBs
					sd_dat_oe <= '1';
					sd_dat_o  <= in_dat(7 downto 4);
					
					-- latch rest of byte & request next-byte
					lsb <= in_dat(3 downto 0);
					if remain>0 then
						in_clk <= '1';
					end if;
					
					-- update status
					state <= S_DATA_L;
				
				when S_DATA_L =>
					-- output LSBs
					sd_dat_oe <= '1';
					sd_dat_o  <= lsb;
					
					-- update status
					remain <= remain-1;
					if remain>0 then
						--> continue with next byte
						state  <= S_DATA_H;
					else
						--> continue with CRC
						state  <= S_CRC;
						remain <= to_unsigned(15,12);
					end if;
				
				when S_CRC =>
					-- output CRC-bits
					sd_dat_oe   <= '1';
					sd_dat_o(0) <= crc0(15);
					sd_dat_o(1) <= crc1(15);
					sd_dat_o(2) <= crc2(15);
					sd_dat_o(3) <= crc3(15);
	
					-- update status
					remain <= remain-1;
					if remain=0 then
						--> done
						state <= S_STOP;
					end if;
					
				when S_STOP =>
					-- output end-bits
					sd_dat_oe <= '1';
					sd_dat_o  <= "1111";
					
					-- update status
					if bremain=0 then
						--> continious read
						state <= S_IDLE;
					elsif bremain>1 then
						--> continue limited read
						bremain <= bremain - 1;
						state <= S_IDLE;
					else
						--> limited read done
						state <= S_INACTIVE;
					end if;
			end case;
			
			-- handle abort-request
			if abort='1' then
				state <= S_INACTIVE;
			end if;

			-- update CRC 
			if state=S_IDLE then
				-- reset CRCs
				crc0 <= x"0000";
				crc1 <= x"0000";
				crc2 <= x"0000";
				crc3 <= x"0000";
			elsif state=S_DATA_H then
				-- update CRCs
				crc0 <= crc16_update(crc0, in_dat(4));
				crc1 <= crc16_update(crc1, in_dat(5));
				crc2 <= crc16_update(crc2, in_dat(6));
				crc3 <= crc16_update(crc3, in_dat(7));
			elsif state=S_DATA_L then
				-- update CRCs
				crc0 <= crc16_update(crc0, lsb(0));
				crc1 <= crc16_update(crc1, lsb(1));
				crc2 <= crc16_update(crc2, lsb(2));
				crc3 <= crc16_update(crc3, lsb(3));
			elsif state=S_CRC then
				-- shift CRCs
				crc0 <= (crc0(14 downto 0) & "0");
				crc1 <= (crc1(14 downto 0) & "0");
				crc2 <= (crc2(14 downto 0) & "0");
				crc3 <= (crc3(14 downto 0) & "0");
			end if;
		end if;
	end process;
	
	-- create stats-flag
	active <= '1' when (state/=S_INACTIVE) else '0';

end rtl;

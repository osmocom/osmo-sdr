
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

entity sdio_cmd_rx is
	port(
		-- common
		clk       : in  std_logic;
		reset     : in  std_logic;
		
		-- SDIO interface
		sd_clk    : in std_logic;
		sd_cmd_i  : in std_logic;
		
		-- control/status
		tx_active : in std_logic;
		
		-- 'event' output
		cmd_ok    : out std_logic;
		cmd_err   : out std_logic;
	
		-- command data
		cmd_ind   : out unsigned(5 downto 0);
		cmd_arg   : out std_logic_vector(31 downto 0)
	);
end sdio_cmd_rx;

architecture rtl of sdio_cmd_rx is

	-- state
	type state_t is (
		S_IDLE,
		S_CMD_DIR,
		S_CMD_IND,
		S_CMD_ARG,
		S_CMD_CRC,
		S_CMD_DONE);
	
	-- status
	signal state  : state_t;
	signal remain : unsigned(4 downto 0);
	
	-- shift-register
	signal tmp_ind : std_logic_vector(5 downto 0);
	signal tmp_arg : std_logic_vector(31 downto 0);
	
	-- CRC
	signal crc7 : std_logic_vector(6 downto 0);
	
begin
	
	-- command receiver
	process(sd_clk,reset)
	begin
		if reset='1' then
			state   <= S_IDLE;
			remain  <= (others=>'0');
			tmp_ind <= (others=>'0');
			tmp_arg <= (others=>'0');
			crc7    <= (others=>'0');
			cmd_ok  <= '0';
			cmd_err <= '0';
			cmd_ind <= (others=>'0');
			cmd_arg <= (others=>'0');
		elsif rising_edge(sd_clk) then
			-- set default values;
			cmd_ok  <= '0';
			cmd_err <= '0';
			
			-- update CRC 
			if sd_cmd_i /= crc7(6)
				then crc7 <= (crc7(5 downto 0) & "0") xor "0001001";
				else crc7 <= (crc7(5 downto 0) & "0");
			end if;
			
			-- update state
			case state is
				when S_IDLE =>
					-- wait for start-bit
					if sd_cmd_i='0' then
						--> start receiving
						state <= S_CMD_DIR;
						crc7  <= (others=>'0');
					end if;
					
				when S_CMD_DIR =>
					-- check direction-bit
					if sd_cmd_i='1' then
						--> start receiving command-index
						state  <= S_CMD_IND;
						remain <= to_unsigned(6-1,5);
					else
						--> should not happen
						state <= S_IDLE;
					end if;
				
				when S_CMD_IND =>
					-- latch bit
					tmp_ind <= tmp_ind(tmp_ind'left-1 downto 0) & sd_cmd_i;
					
					-- update status
					remain <= remain-1;
					if remain=0 then
						state  <= S_CMD_ARG;
						remain <= to_unsigned(32-1,5);
					end if;
				
				when S_CMD_ARG =>
					-- latch bit
					tmp_arg <= tmp_arg(tmp_arg'left-1 downto 0) & sd_cmd_i;
				
					-- update status
					remain <= remain-1;
					if remain=0 then
						state  <= S_CMD_CRC;
						remain <= to_unsigned(7-1,5);
					end if;
					
				when S_CMD_CRC =>
					-- update status
					remain <= remain-1;
					if remain=0 then
						state <= S_CMD_DONE;
					end if;
					
				when S_CMD_DONE =>
					-- verify CRC
					if crc7="0000000" then
						--> CRC ok
						cmd_ok <= '1';
						cmd_ind <= unsigned(tmp_ind);
						cmd_arg <= tmp_arg;
					else
						--> CRC error
						cmd_err <= '1';
					end if;
					
					-- return to idle state
					state <= S_IDLE;
			end case;
			
			-- always return to idle state when transmitter is active
			if tx_active='1' then
				state <= S_IDLE;
			end if;
			
		end if;
	end process;
	
end rtl;

-------------------------------------------------------------------------------

library ieee;
	use ieee.std_logic_1164.all;
	use ieee.numeric_std.all;
library work;
	use work.all;
	use work.mt_toolbox.all;

entity sdio_cmd_tx is
	port(
		-- common
		clk       : in  std_logic;
		reset     : in  std_logic;
		
		-- SDIO interface
		sd_clk    : in  std_logic;
		sd_cmd_oe : out std_logic;
		sd_cmd_o  : out std_logic;
		
		-- control/status
		tx_active : out std_logic;
		
		-- response control
		res_start : in  std_logic;
		res_done  : out std_logic;
		
		-- response data
		res_ind   : in  unsigned(5 downto 0);
		res_arg   : in  std_logic_vector(31 downto 0)
	);

	attribute syn_useioff : boolean;
	attribute syn_useioff of sd_cmd_oe : signal is true; 
	attribute syn_useioff of sd_cmd_o  : signal is true; 

end sdio_cmd_tx;

architecture rtl of sdio_cmd_tx is

	-- state
	type state_t is (
		S_IDLE,
		S_RES_DIR,
		S_RES_IND,
		S_RES_ARG,
		S_RES_CRC,
		S_RES_DONE
	);
	
	-- status
	signal state  : state_t;
	signal remain : unsigned(4 downto 0);
	
	-- shift-register
	signal sreg : std_logic_vector(31 downto 0);
	
	-- CRC
	signal crc7 : std_logic_vector(6 downto 0);
	
begin
	
	-- command receiver
	process(sd_clk,reset)
	begin
		if reset='1' then
			state     <= S_IDLE;
			remain    <= (others=>'0');
			sreg      <= (others=>'1');
			crc7      <= (others=>'0');
			sd_cmd_oe <= '0';
			sd_cmd_o  <= '0';
			res_done  <= '0';
			tx_active <= '0';
		elsif rising_edge(sd_clk) then
			-- set default values;
			sd_cmd_oe <= '0';
			sd_cmd_o  <= '0';
			res_done  <= '0';
			
			-- update state
			case state is
				when S_IDLE =>
					-- wait for start-request
					if res_start='1' then
						--> start transmitting
						state <= S_RES_DIR;
						
						-- write start-bit
						sd_cmd_oe <= '1';
						sd_cmd_o  <= '0';
					end if;
					
				when S_RES_DIR =>
					-- write direction-bit
					sd_cmd_oe <= '1';
					sd_cmd_o  <= '0';
					
					-- prepare writing command-index
					sreg(31 downto 26) <= std_logic_vector(res_ind);
					
					-- update status
					state  <= S_RES_IND;
					remain <= to_unsigned(6-1,5);
				
				when S_RES_IND =>
					-- write next bit
					sd_cmd_oe <= '1';
					sd_cmd_o  <= sreg(31);
					
					-- update shift-register
					sreg <= sreg(30 downto 0) & '1';
					
					-- update status
					remain <= remain-1;
					if remain=0 then
						sreg   <= res_arg;
						state  <= S_RES_ARG;
						remain <= to_unsigned(32-1,5);
					end if;
				
				when S_RES_ARG =>
					-- write next bit
					sd_cmd_oe <= '1';
					sd_cmd_o  <= sreg(31);
					
					-- update shift-register
					sreg <= sreg(30 downto 0) & '1';
					
					-- update status
					remain <= remain-1;
					if remain=0 then
						state  <= S_RES_CRC;
						remain <= to_unsigned(7-1,5);
					end if;
					
				when S_RES_CRC =>
					-- write CRC bit
					sd_cmd_oe <= '1';
					sd_cmd_o  <= crc7(6);
					
					-- update status
					remain <= remain-1;
					if remain=0 then
						state <= S_RES_DONE;
					end if;
					
				when S_RES_DONE =>
					-- write end-bit
					sd_cmd_oe <= '1';
					sd_cmd_o  <= '1';
					
					-- return to idle state
					res_done <= '1';
					state <= S_IDLE;
			end case;
			
			-- update CRC 
			if state=S_IDLE then
				crc7 <= (others=>'0');
			elsif state=S_RES_IND or state=S_RES_ARG then
				if sreg(31) /= crc7(6)
					then crc7 <= (crc7(5 downto 0) & "0") xor "0001001";
					else crc7 <= (crc7(5 downto 0) & "0");
				end if;
			elsif state=S_RES_CRC then
				crc7 <= (crc7(5 downto 0) & "0");
			end if;
			
			-- create active-flag
			if state/=S_IDLE 
				then tx_active <= '1';
				else tx_active <= '0';
			end if;
		end if;
	end process;
end rtl;

-------------------------------------------------------------------------------

library ieee;
	use ieee.std_logic_1164.all;
	use ieee.numeric_std.all;
library work;
	use work.all;
	use work.mt_toolbox.all;

entity sdio_cmd_handler is
	port(
		-- common
		clk        : in  std_logic;
		reset      : in  std_logic;
		sd_clk     : in  std_logic;
		
		-- status
		dtx_active : in std_logic;
		
		-- command receiver
		cmd_ok     : in  std_logic;
		cmd_err    : in  std_logic;
		cmd_ind    : in  unsigned(5 downto 0);
		cmd_arg    : in  std_logic_vector(31 downto 0);
			
		-- response transmitter
		res_start  : out std_logic;
		res_done   : in  std_logic;
		res_ind    : out unsigned(5 downto 0);
		res_arg    : out std_logic_vector(31 downto 0);
		
		-- function0 - register-interface
		fn0_rena   : out std_logic;
		fn0_wena   : out std_logic;
		fn0_addr   : out unsigned(16 downto 0);
		fn0_oor    : in  std_logic;
		fn0_rdat   : in  std_logic_vector(7 downto 0);
		fn0_wdat   : out std_logic_vector(7 downto 0);
		
		-- function1 - control
		fn1_start  : out std_logic;
		fn1_count  : out unsigned(8 downto 0)
	);
end sdio_cmd_handler;

architecture rtl of sdio_cmd_handler is

	-- state
	type state_t is (
		S_IDLE,
		S_REG_READ1,
		S_REG_READ2,
		S_REG_WRITE1,
		S_REG_WRITE2,
		S_REG_RAW,
		S_DUMMY_RESPONSE
	);
	
	-- status
	signal state : state_t;
	
	-- status flags
	signal flags    : std_logic_vector(7 downto 0);
	signal flag_crc : std_logic; -- COM_CRC_ERROR
	signal flag_cmd : std_logic; -- ILLEGAL_COMMAND
	signal flag_err : std_logic; -- ERROR 
	signal flag_fun : std_logic; -- FUNCTION_NUMBER 
	signal flag_oor : std_logic; -- OUT_OF_RANGE
	
begin
	
	-- assemble status-flags
	flags(7) <= flag_crc;
	flags(6) <= flag_cmd;
	flags(5) <= dtx_active;
	flags(4) <= '0';
	flags(3) <= flag_err;
	flags(2) <= '0';
	flags(1) <= flag_fun;
	flags(0) <= flag_oor;
	
	-- control logic
	process(sd_clk,reset)
		variable func : unsigned(2 downto 0);
	begin
		if reset='1' then
			res_start <= '0';
			res_ind   <= (others=>'0');
			res_arg   <= (others=>'0');
			state     <= S_IDLE;
			flag_crc  <= '0';
			flag_cmd  <= '0';
			flag_err  <= '0';
			flag_fun  <= '0';
			flag_oor  <= '0';
			fn0_rena  <= '0';
			fn0_wena  <= '0';
			fn0_addr  <= (others=>'0');
			fn0_wdat  <= (others=>'0');
			fn1_start <= '0';
			fn1_count <= (others=>'0');
		elsif rising_edge(sd_clk) then
			-- set default-values
			res_start <= '0';
			fn1_start <= '0';
			
			-- react on command
			if cmd_ok='1' then
				-- clear error-flags by default
				flag_fun <= '0';
				flag_oor <= '0';
				flag_err <= '0';
				
				-- check command-index
				if cmd_ind=52 then
					--> IO_RW_DIRECT
					
					-- get function
					func := unsigned(cmd_arg(30 downto 28));
					if func=0 then
						-- decode rest of argument
						fn0_addr <= unsigned(cmd_arg(25 downto 9));
						fn0_wdat <= cmd_arg(7 downto 0);
						fn0_rena <= not cmd_arg(31);
						fn0_wena <= cmd_arg(31);
						
						-- update status
						if cmd_arg(31)='0' then
							state <= S_REG_READ1;
						elsif cmd_arg(27)='0' then
							state <= S_REG_WRITE1;
						else 
							state <= S_REG_RAW;
						end if;
					else
						-- invalid function number
						flag_fun <= '1';
						state <= S_DUMMY_RESPONSE;
					end if;
				elsif cmd_ind=53 then
					--> IO_RW_EXTENDED
					
					-- get function
					func := unsigned(cmd_arg(30 downto 28));
					if func=1 then
						-- check direction
						if cmd_arg(31)='0' and cmd_arg(27)='1' then
							-- decode rest of argument
							fn1_start <= '1';
							fn1_count <= unsigned(cmd_arg(8 downto 0));
							
							-- create response
							state <= S_DUMMY_RESPONSE;
						else
							-- unsupported mode
							flag_err <= '1';
							state <= S_DUMMY_RESPONSE;
						end if;
					else
						-- invalid function number
						flag_fun <= '1';
						state <= S_DUMMY_RESPONSE;
					end if;
				else
					-- unknown command -> ignore
					flag_cmd <= '1';
				end if;
			elsif cmd_err='1' then
				--> error
				flag_crc <= '1';
			end if;
			
			-- update status
			case state is
				when S_IDLE =>
					null;
				
				when S_REG_READ1 =>
					-- wait-state
					state <= S_REG_READ2;
					
				when S_REG_WRITE1 =>
					-- wait-state
					state <= S_REG_READ2;
				
				when S_REG_READ2 =>
					if fn0_oor='0' then
						-- assemble response with read-data
						res_start <= '1';
						res_ind <= cmd_ind;
						res_arg(31 downto 16) <= x"0000";
						res_arg(15 downto  8) <= flags;
						res_arg( 7 downto  0) <= fn0_rdat;
						state <= S_IDLE;
					else
						-- invalid address, report error
						flag_oor <= '1';
						state <= S_DUMMY_RESPONSE;
					end if;
				
				when S_REG_WRITE2 =>
					if fn0_oor='0' then
						-- assemble response with write-data
						res_start <= '1';
						res_ind <= cmd_ind;
						res_arg(31 downto 16) <= x"0000";
						res_arg(15 downto  8) <= flags;
						res_arg( 7 downto  0) <= cmd_arg(7 downto 0);
						state <= S_IDLE;
					else
						-- invalid address, report error
						flag_oor <= '1';
						state <= S_DUMMY_RESPONSE;
					end if;
				
				when S_REG_RAW =>
					-- read-after-write -> issue read-request now
					fn0_rena <= '1';
					state <= S_REG_READ1;
					
				when S_DUMMY_RESPONSE =>
					-- assemble dummy-response (==> only flags)
					res_start <= '1';
					res_ind <= cmd_ind;
					res_arg(31 downto 16) <= x"0000";
					res_arg(15 downto  8) <= flags;
					res_arg( 7 downto  0) <= x"00";
					state <= S_IDLE;
				
			end case;
			
			-- clear some status-flags after response was transmitter
			if res_done='1' then
				flag_crc <= '0';
				flag_cmd <= '0';
			end if;
		end if;
	end process;
end rtl;

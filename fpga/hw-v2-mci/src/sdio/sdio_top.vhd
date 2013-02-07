
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

entity sdio_top is
	port (
		-- common
		clk      : in  std_logic;
		reset    : in  std_logic;
		
		-- input
		in_clk   : in  std_logic;
		in_data  : in  std_logic_vector(31 downto 0);
		
		-- SDIO interface
		sd_clk   : in    std_logic;
		sd_cmd   : inout std_logic;
		sd_dat   : inout std_logic_vector(3 downto 0)
	);
end sdio_top;

architecture rtl of sdio_top is
	
	-- spitted tristate pads
	signal sd_cmd_i  : std_logic;
	signal sd_cmd_o  : std_logic;
	signal sd_cmd_oe : std_logic;
	signal sd_dat_i  : std_logic_vector(3 downto 0);
	signal sd_dat_o  : std_logic_vector(3 downto 0);
	signal sd_dat_oe : std_logic;
	
	-- status/control
	signal ctx_active : std_logic;
	signal dtx_active : std_logic;
	
	-- command receiver
	signal cmd_ok  : std_logic;
	signal cmd_err : std_logic;
	signal cmd_ind : unsigned(5 downto 0);
	signal cmd_arg : std_logic_vector(31 downto 0);
		
	-- response transmitter
	signal res_start : std_logic;
	signal res_done  : std_logic;
	signal res_ind   : unsigned(5 downto 0);
	signal res_arg   : std_logic_vector(31 downto 0);
	
	-- bus-interface
	signal bus_rena  : std_logic;
	signal bus_wena  : std_logic;
	signal bus_addr  : unsigned(16 downto 0);
	signal bus_oor   : std_logic;
	signal bus_rdat  : std_logic_vector(7 downto 0);
	signal bus_wdat  : std_logic_vector(7 downto 0);
	
	-- registers
	signal fn1_start : std_logic;
	signal fn1_abort : std_logic;
	signal fn1_count : unsigned(8 downto 0);
	signal fn1_bsize : unsigned(15 downto 0);
	
	-- TX-control <-> TX-core
	signal tx_ready : std_logic;
	signal tx_sync  : std_logic;
	signal tx_clk   : std_logic;
	signal tx_dat   : std_logic_vector(7 downto 0);
	
begin
	
	-- create tristate pads
	sd_cmd_i <= to_X01(sd_cmd);
	sd_dat_i <= to_X01(sd_dat);
	sd_cmd <= sd_cmd_o when sd_cmd_oe='1' else 'Z';
	sd_dat <= sd_dat_o when sd_dat_oe='1' else "ZZZZ";

	
	--
	-- control logic
	--
	
	process(sd_clk,reset)
	begin
		if reset='1' then
			bus_oor   <= '0';
			bus_rdat  <= x"00";
			fn1_bsize <= x"0200";
			fn1_abort <= '0';
		elsif rising_edge(sd_clk) then
			-- set default values
			bus_oor   <= '0';
			bus_rdat  <= x"00";
			fn1_abort <= '0';
			
			-- handle request
			case to_integer(bus_addr) is
				-- some random static registers
				when 0 =>
					bus_rdat <= x"43";
				when 1 =>
					bus_rdat <= x"03";
				
				-- IO Abort
				when 6 =>
					if bus_wena='1' and bus_wdat(2 downto 0)="001" then
						fn1_abort <= '1';
					end if;
				
				-- FN1 blocksize
				when 16#110# =>
					bus_rdat <= to_slv8(fn1_bsize(7 downto 0));
					if bus_wena='1' then
						fn1_bsize(7 downto 0) <= unsigned(bus_wdat);
					end if;
				when 16#111# =>
					bus_rdat <= to_slv8(fn1_bsize(15 downto 8));
					if bus_wena='1' then
						fn1_bsize(15 downto 8) <= unsigned(bus_wdat);
					end if;
					
				when others =>
					-- unknown register
					bus_oor <= '1';
			end case;
		end if;
	end process;
	
	
	--
	-- components
	--
	
	-- command receiver
	crx: entity sdio_cmd_rx
		port map (
			-- common
			clk      => clk,
			reset    => reset,
			
			-- SDIO interface
			sd_clk   => sd_clk,
			sd_cmd_i => sd_cmd_i,
			
			-- status/control
			tx_active => ctx_active,
	
			-- 'event' output
			cmd_ok   => cmd_ok,
			cmd_err  => cmd_err,
			
			-- command data
			cmd_ind  => cmd_ind,
			cmd_arg  => cmd_arg
		);
	
	-- response transmitter
	ctx: entity sdio_cmd_tx
		port map (
			-- common
			clk       => clk,
			reset     => reset,
			
			-- SDIO interface
			sd_clk    => sd_clk,
			sd_cmd_oe => sd_cmd_oe,
			sd_cmd_o  => sd_cmd_o,
			
			-- status/control
			tx_active => ctx_active,
			
			-- response control
			res_start => res_start,
			res_done  => res_done,
			
			-- response data
			res_ind   => res_ind,
			res_arg   => res_arg
		);
	
	-- command handler
	cmd: entity sdio_cmd_handler
		port map (
			-- common
			clk        => clk,
			reset      => reset,
			sd_clk     => sd_clk,
			
			-- status
			dtx_active => dtx_active,
		
			-- command receiver
			cmd_ok     => cmd_ok,
			cmd_err    => cmd_err,
			cmd_ind    => cmd_ind,
			cmd_arg    => cmd_arg,
				
			-- response transmitter
			res_start  => res_start,
			res_done   => res_done,
			res_ind    => res_ind,
			res_arg    => res_arg,
			
			-- function0 - register-interface
			fn0_rena   => bus_rena,
			fn0_wena   => bus_wena,
			fn0_addr   => bus_addr,
			fn0_oor    => bus_oor,
			fn0_rdat   => bus_rdat,
			fn0_wdat   => bus_wdat,
			
			-- function1 - control
			fn1_start  => fn1_start,
			fn1_count  => fn1_count
		);

	-- data transmitter
	dtx: entity sdio_dat_tx
		port map (
			-- common
			clk       => clk,
			reset     => reset,
			
			-- control
			start     => fn1_start,
			abort     => fn1_abort,
			bcount    => fn1_count,
			bsize     => fn1_bsize,
			
			-- status
			active    => dtx_active,
		
			-- input data
			in_ready  => tx_ready,
			in_sync   => tx_sync,
			in_clk    => tx_clk,
			in_dat    => tx_dat,
			
			-- SDIO interface
			sd_clk    => sd_clk,
			sd_dat_oe => sd_dat_oe,
			sd_dat_o  => sd_dat_o
		);

	-- TX data control/fifo
	txctl: entity sdio_txctrl
		port map (
			-- clocks
			clk_sys   => clk,
			rst_sys   => reset,
			sd_clk    => sd_clk,
			
			-- config
			cfg_bsize => fn1_bsize,
			
			-- input
			in_clk    => in_clk,
			in_data   => in_data,
			
			-- output
			out_ready => tx_ready,
			out_sync  => tx_sync,
			out_clk   => tx_clk,
			out_dat   => tx_dat
		);

end rtl;
---------------------------------------------------------------------------------------------------
-- Filename    : usbrx_clkref.vhd
-- Project     : OsmoSDR FPGA Firmware
-- Purpose     : Reference Clock Measurement
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

entity usbrx_clkref is
	port(
		-- system clocks
		clk_sys  : in  std_logic;
		rst_sys  : in  std_logic;
		
		-- reference signal
		clk_ref  : in  std_logic;
		rst_ref  : in  std_logic;
		
		-- 1pps signal
		gps_1pps : in  std_logic;
		
		-- status
		status   : out usbrx_ref_status_t
	);
end usbrx_clkref;

architecture rtl of usbrx_clkref is

	-- deglitcher
	signal dgl_hist : std_logic_vector(3 downto 0);
	signal dgl_out  : std_logic;
	
	-- edge detection
	signal pps_last : std_logic;

	-- active counters
	signal cnt_lsb : unsigned(24 downto 0);
	
	-- latched counters
	signal lat_upd : std_logic;
	signal lat_lsb : unsigned(24 downto 0);
	signal lat_msb : unsigned(6 downto 0);

	-- output register
	signal out_upd : std_logic;
	signal out_lsb : unsigned(24 downto 0);
	signal out_msb : unsigned(6 downto 0);
	
begin
	
	-- reference counter
	process(clk_ref)
		variable cnt0,cnt1 : natural range 0 to 4;
		variable re : boolean;
	begin
		if rising_edge(clk_ref) then
			-- set default values
			lat_upd <= '0';
			
			-- deglitch pps signal
			dgl_hist <= dgl_hist(dgl_hist'left-1 downto 0) & gps_1pps;
			cnt0 := 0;
			cnt1 := 0;
			for i in dgl_hist'range loop
				if dgl_hist(i)='0' then
					cnt0 := cnt0 + 1;
				end if;
				if dgl_hist(i)='1' then
					cnt1 := cnt1 + 1;
				end if;
			end loop;
			if cnt0 >= 3 then
				dgl_out <= '0';
			elsif cnt1 >= 3 then
				dgl_out <= '1';
			end if;
			
			-- detect rising edge on pps
			pps_last <= dgl_out;
			re := (dgl_out='1' and pps_last='0');
			
			-- update counters
			if re then
				cnt_lsb <= to_unsigned(0, cnt_lsb'length);
				lat_lsb <= cnt_lsb;
				lat_msb <= lat_msb + 1;
				lat_upd <= '1';
			else
				cnt_lsb <= cnt_lsb + 1;
			end if;

			-- handle reset
			if rst_ref='1' then
				dgl_hist <= (others=>'0');
				dgl_out  <= '0';
				pps_last <= '0';
				cnt_lsb  <= (others=>'0');
				lat_upd  <= '0';
				lat_lsb  <= (others=>'0');
				lat_msb  <= (others=>'0');
			end if;
		end if;
	end process;

	-- bring update-pulse into correct clock domain
	syn: entity mt_sync_feedback
		port map (
			i_clk  => clk_ref,
			i_data => lat_upd,
			o_clk  => clk_sys,
			o_data => out_upd
		);
	
	-- output register
	process(clk_sys)
	begin
		if rising_edge(clk_sys) then
			-- update output when requested
			if out_upd='1' then
				out_lsb  <= lat_lsb;
				out_msb  <= lat_msb;
			end if;
			
			-- handle reset
			if rst_sys='1' then
				out_lsb  <= (others=>'0');
				out_msb  <= (others=>'0');
			end if;
		end if;
	end process;
	
	-- output status
	status.lsb <= out_lsb;
	status.msb <= out_msb;
end rtl;

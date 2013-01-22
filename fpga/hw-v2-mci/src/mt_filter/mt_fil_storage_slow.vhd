---------------------------------------------------------------------------------------------------
-- Filename    : mt_fil_storage_slow.vhd
-- Project     : maintech filter toolbox
-- Purpose     : basic data storage for FIR-like filters
--               - version for 'slow' filter versions
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
-- mt_fil_dstorage_slow ------------------------------------------------------
-------------------------------------------------------------------------------

library ieee;
	use ieee.std_logic_1164.all;
	use ieee.numeric_std.all;
library work;
	use work.all;
	use work.mt_toolbox.all;
	use work.mt_filter.all;

entity mt_fil_dstorage_slow is
	generic (
		CHANNELS : natural;
		DEPTH    : natural;
		RAMSTYLE : string
	);
	port (
		-- common
		clk 	: in  std_logic;
		reset 	: in  std_logic;
		
		-- control
		chan    : in  unsigned(log2(CHANNELS)-1 downto 0);
		load	: in  std_logic;
		start	: in  std_logic;
		stop    : in  std_logic;
		active	: in  std_logic;
		
		-- datapath
		din		: in  fir_dataword18;
		dout1	: out fir_dataword18;
		dout2	: out fir_dataword18
	);
end mt_fil_dstorage_slow;

architecture rtl of mt_fil_dstorage_slow is

	--
	-- types & rams
	--

	-- derived constants
	constant MEMSIZE : natural := CHANNELS * DEPTH;

	-- internal types
	subtype offset_t is unsigned(log2(DEPTH)-1 downto 0);
	subtype addr_t is unsigned(log2(MEMSIZE)-1 downto 0);
	subtype data_t is fir_dataword18;
	type atab_t is array(CHANNELS-1 downto 0) of addr_t;
	type pram_t is array(CHANNELS-1 downto 0) of offset_t;
	type sram_t is array(MEMSIZE-1 downto 0) of data_t;
	
	-- create address tables
	function get_addr_tab return atab_t is 
		variable res : atab_t;
	begin
		for i in res'range loop
			res(i) := to_unsigned(i*DEPTH, addr_t'length);
		end loop;
		return res;
	end get_addr_tab;
	constant addr_tab : atab_t := get_addr_tab;
	
	-- ram ports
	signal sram1_we    : std_logic;
	signal sram1_waddr : addr_t;
	signal sram1_wdata : data_t;
	signal sram1_re    : std_logic;
	signal sram1_raddr : addr_t;
	signal sram1_rdata : data_t := (others=>'0');
	signal sram2_we    : std_logic;
	signal sram2_waddr : addr_t;
	signal sram2_wdata : data_t;
	signal sram2_re    : std_logic;
	signal sram2_raddr : addr_t;
	signal sram2_rdata : data_t := (others=>'0');
	
	-- actual rams
	signal pram  : pram_t := (others=>(others=>'0'));
	signal sram1 : sram_t := (others=>(others=>'0'));
	signal sram2 : sram_t := (others=>(others=>'0'));
	
	-- configure rams
	attribute syn_ramstyle of pram  : signal is "logic";
	attribute syn_ramstyle of sram1 : signal is RAMSTYLE;
	attribute syn_ramstyle of sram2 : signal is RAMSTYLE&",no_rw_check";
	
	
	--
	-- status
	--
	
	-- delayed control signals
	signal start_del   : std_logic_vector(1 downto 0);
	signal load_del    : std_logic_vector(2 downto 0);
	signal active_del  : std_logic_vector(1 downto 0);
	signal stop_del    : std_logic_vector(2 downto 0);
	
	-- status
	signal selchan     : unsigned(log2(CHANNELS)-1 downto 0);
	signal baseaddr    : addr_t;
	signal woffset     : offset_t;
	signal roffset1    : offset_t;
	signal roffset2    : offset_t;
	
begin
	
	-- validate generics
	assert DEPTH>1
		report "mt_fil_dstorage_slow: DEPTH must be larger than 1"
		severity FAILURE;
	
	-- control logic
	process(clk, reset)
		variable offset : offset_t;
	begin
		if reset='1' then
			start_del   <= (others=>'0');
			load_del    <= (others=>'0');
			active_del  <= (others=>'0');
			stop_del    <= (others=>'0');
			selchan     <= (others=>'0');
			baseaddr    <= (others=>'0');
			woffset     <= (others=>'0');
			roffset1    <= (others=>'0');
			roffset2    <= (others=>'0');
			sram1_re    <= '0';
			sram1_we    <= '0';
			sram1_raddr <= (others=>'0');
			sram1_waddr <= (others=>'0');
			sram1_wdata <= (others=>'0');
			sram2_re    <= '0';
			sram2_we    <= '0';
			sram2_raddr <= (others=>'0');
			sram2_waddr <= (others=>'0');
			sram2_wdata <= (others=>'0');
		elsif rising_edge(clk) then 
			-- set default values
			sram1_re <= '0';
			sram1_we <= '0';
			sram2_re <= '0';
			sram2_we <= '0';
			
			-- create delayed flags
			start_del  <= start_del(start_del'left-1 downto 0)   & start;
			load_del   <= load_del(load_del'left-1 downto 0)     & load;
			active_del <= active_del(active_del'left-1 downto 0) & active;
			stop_del   <= stop_del(stop_del'left-1 downto 0)     & stop;

			-- init status on start of burst
			if start='1' then
				-- remember channel
				selchan  <= chan;
				
				-- get base-address for selected channels
				baseaddr <= addr_tab(to_integer(chan));
				
				-- init pointers
				offset   := pram(to_integer(chan));
				woffset  <= offset;
				roffset1 <= offset;
				if offset=(DEPTH-1)
					then roffset2 <= to_unsigned(0,roffset2'length);
					else roffset2 <= offset + 1;
				end if;
			end if;
			
			-- store sample into ram and increment write-pointer if 'load'-flag is set
			if load_del(0)='1' then
				-- write sample into ram
				sram1_we    <= '1';
				sram1_waddr <= baseaddr + woffset;
				sram1_wdata <= din;
				
				-- update write-pointer
				woffset <= roffset2; -- 'roffset2' is actually "((woffset+1) mod DEPTH)" here
			end if;
			if load_del(1)='1' then
				-- write-back updated write-pointer
				pram(to_integer(selchan)) <= woffset;
			end if;
			
			-- carry sample from sram1 into sram2 if 'stop'-flag is set
			if load_del(1)='1' then
				sram2_waddr <= baseaddr + woffset;
			end if;
			if stop_del(2)='1' then
				sram2_we    <= '1';
				sram2_wdata <= sram1_rdata;
			end if;
			
			-- issue read-requests when active
			if active_del(0)='1' then
				-- read samples from ram
				sram1_re    <= '1';
				sram2_re    <= '1';
				sram1_raddr <= baseaddr + roffset1;
				sram2_raddr <= baseaddr + roffset2;
				
				-- update read-pointers
				if roffset1=0
					then roffset1 <= to_unsigned(DEPTH-1,roffset1'length);
					else roffset1 <= roffset1 - 1;
				end if;
				if roffset2=(DEPTH-1)
					then roffset2 <= to_unsigned(0,roffset2'length);
					else roffset2 <= roffset2 + 1;
				end if;
			end if;
			
		end if;
	end process;
	
	-- set output
	dout1 <= sram1_rdata when load_del(2)='0' else din;
	dout2 <= sram2_rdata;
	
	-- infer rams
	process(clk)
	begin
		if rising_edge(clk) then 
			if sram1_we='1' then
				sram1(to_integer(sram1_waddr)) <= sram1_wdata;
			end if;
			if sram1_re='1' then
				sram1_rdata <= sram1(to_integer(sram1_raddr));
			end if;
			if sram2_we='1' then
				sram2(to_integer(sram2_waddr)) <= sram2_wdata;
			end if;
			if sram2_re='1' then
				sram2_rdata <= sram2(to_integer(sram2_raddr));
			end if;
		end if;
	end process;

end rtl;



-------------------------------------------------------------------------------
-- mt_fil_storage_slow --------------------------------------------------------
-------------------------------------------------------------------------------

library ieee;
	use ieee.std_logic_1164.all;
	use ieee.numeric_std.all;
library work;
	use work.all;
	use work.mt_toolbox.all;
	use work.mt_filter.all;

entity mt_fil_storage_slow is
	generic (
		COEFFS     : fir_coefficients;		-- coefficients
		DCHAN      : natural;				-- number of data channels
		TAPS       : natural;				-- number of samples in each segment
		RAMSTYLE   : string;				-- ram style for inferred memories
		ROMSTYLE   : string					-- ram style for coefficent rom
	);
	port (
		-- common
		clk 	   : in  std_logic;
		reset 	   : in  std_logic;
		
		-- config
		chan       : in  unsigned(log2(DCHAN)-1 downto 0);
		
		-- input
		in_load	   : in  std_logic;
		in_start   : in  std_logic;
		in_stop    : in  std_logic;
		in_active  : in  std_logic;
		in_data	   : in  fir_dataword18;
		
		-- output
		out_load   : out std_logic;
		out_start  : out std_logic;
		out_stop   : out std_logic;
		out_active : out std_logic;
		out_data1  : out fir_dataword18;
		out_data2  : out fir_dataword18;
		out_coeff  : out fir_dataword18
	);
end mt_fil_storage_slow;

architecture rtl of mt_fil_storage_slow is

	-- status
	signal del_load   : std_logic_vector(1 downto 0);
	signal del_start  : std_logic_vector(1 downto 0);
	signal del_stop   : std_logic_vector(1 downto 0);
	signal del_active : std_logic_vector(1 downto 0);
	signal cind       : unsigned(log2(TAPS)-1 downto 0);
	
	-- coeff rom
	constant rom_size : natural := 1 * (2**log2(TAPS));
	type rom_t is array (0 to rom_size-1) of fir_dataword18;
	function generate_rom return rom_t is
		variable rom : rom_t;
		variable ssize : natural;
	begin
		ssize := 2**log2(TAPS);
		rom := (others=>(others=>'0'));
		for t in 0 to TAPS-1 loop
			rom(t) := to_signed(COEFFS(t), 18);
		end loop;
		return rom;
	end generate_rom;
	signal rom : rom_t := generate_rom;
	
	-- don't waste blockram 
	attribute syn_romstyle of rom : signal is ROMSTYLE;
	attribute syn_ramstyle of rom : signal is ROMSTYLE;
	
begin
	
	-- data-buffer
	dbuf: entity mt_fil_dstorage_slow
		generic map (
			CHANNELS => DCHAN,
			DEPTH    => TAPS,
			RAMSTYLE => RAMSTYLE
		)
		port map (
			clk    => clk,
			reset  => reset,
			chan   => chan,
			load   => in_load,
			start  => in_start,
			stop   => in_stop,
			active => in_active,
			din	   => in_data,
			dout1  => out_data1,
			dout2  => out_data2
		);
	
	-- control logic
	process(clk, reset)
	begin
		if reset='1' then
			del_load   <= (others=>'0');
			del_start  <= (others=>'0');
			del_stop   <= (others=>'0');
			del_active <= (others=>'0');
			out_load   <= '0';
			out_start  <= '0';
			out_stop   <= '0';
			out_active <= '0';
			out_coeff  <= (others=>'0');
			cind       <= (others=>'0');
		elsif rising_edge(clk) then
			-- create delayed control flags
			del_load   <= del_load(del_load'left-1 downto 0)     & in_load;
			del_start  <= del_start(del_start'left-1 downto 0)   & in_start;
			del_stop   <= del_stop(del_stop'left-1 downto 0)     & in_stop;
			del_active <= del_active(del_active'left-1 downto 0) & in_active;
			
			-- output delayed control flags
			out_load   <= del_load(1);
			out_start  <= del_start(1);
			out_stop   <= del_stop(1);
			out_active <= del_active(1);
			
			-- update coeff-indices
			if del_start(0)='1' then
				cind <= to_unsigned(0, cind'length);
			elsif del_active(0)='1' then
				cind <= cind + 1;
			end if;
			
			-- output coefficent
			out_coeff <= rom(to_integer(cind));
		end if;
	end process;
		
end rtl;

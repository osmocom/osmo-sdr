---------------------------------------------------------------------------------------------------
-- Filename    : usbrx_halfband.vhd
-- Project     : OsmoSDR FPGA Firmware
-- Purpose     : Multichannel Halfband Decimation Filter
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
-- halfband decimation filter - input cache/control ---------------------------
-------------------------------------------------------------------------------

library ieee;
	use ieee.std_logic_1164.all;
	use ieee.numeric_std.all;
library work;
	use work.all;
	use work.mt_toolbox.all;
	use work.mt_filter.all;

entity usbrx_halfband_ictrl is
	generic (
		N        : natural := 3
	);
	port (
		-- common
		clk      : in  std_logic;
		reset    : in  std_logic;
		
		-- input
		in_clk   : in  std_logic_vector(N-1 downto 0);
		in_i     : in  fir_databus18(N-1 downto 0);
		in_q     : in  fir_databus18(N-1 downto 0);
		
		-- output
		out_ready : in  std_logic;
		out_clk   : out std_logic;
		out_chan  : out unsigned(log2(N)-1 downto 0);
		out_i0    : out fir_dataword18;
		out_i1    : out fir_dataword18;
		out_q0    : out fir_dataword18;
		out_q1    : out fir_dataword18
	);
end usbrx_halfband_ictrl;

architecture rtl of usbrx_halfband_ictrl is	

	-- control
	signal phase : std_logic_vector(N-1 downto 0);
	signal tmp_i : fir_databus18(N-1 downto 0);
	signal tmp_q : fir_databus18(N-1 downto 0);
	signal oclk  : std_logic;
	
	-- output
	signal pending  : std_logic_vector(N-1 downto 0);
	signal cache_i0 : fir_databus18(N-1 downto 0);
	signal cache_i1 : fir_databus18(N-1 downto 0);
	signal cache_q0 : fir_databus18(N-1 downto 0);
	signal cache_q1 : fir_databus18(N-1 downto 0);
	
begin
	
	-- control logic
	process(clk)
		variable found : std_logic;
	begin
		if rising_edge(clk) then
			-- set default values
			oclk <= '0';
			
			-- check if we are allowed to output the next entry
			if out_ready='1' and oclk='0' then
				-- search for pending cache-entry
				found := '0';
				for i in 0 to N-1 loop
					if found='0' and pending(i)='1' then
						--> output entry
						oclk <= '1';
						out_chan <= to_unsigned(i,out_chan'length);
						out_i0 <= cache_i0(i);
						out_i1 <= cache_i1(i);
						out_q0 <= cache_q0(i);
						out_q1 <= cache_q1(i);
						
						-- update status
						pending(i) <= '0';
						found := '1';
					end if;
				end loop;
			end if;
			
			-- handle incoming samples
			for i in in_clk'range loop
				if in_clk(i)='1' then
					-- write sample into cache
					if phase(i)='0' then 
						tmp_i(i) <= in_i(i);
						tmp_q(i) <= in_q(i);
					else 
						pending(i)  <= '1';
						cache_i0(i) <= tmp_i(i);
						cache_q0(i) <= tmp_q(i);
						cache_i1(i) <= in_i(i);
						cache_q1(i) <= in_q(i);	
					end if;
					phase(i) <= not phase(i);
				
					-- debug check
					assert phase(i)='0' or pending(i)='0'
						report "usbrx_halfband_ictrl: input too fast" 
						severity error;
				end if;
			end loop;
			
			-- handle reset
			if reset='1' then
				phase    <= (others=>'0');
				tmp_i    <= (others=>(others=>'0'));
				tmp_q    <= (others=>(others=>'0'));
				pending  <= (others=>'0');
				cache_i0 <= (others=>(others=>'0'));
				cache_i1 <= (others=>(others=>'0'));
				cache_q0 <= (others=>(others=>'0'));
				cache_q1 <= (others=>(others=>'0'));
				oclk     <= '0';
				out_chan <= (others=>'0');
				out_i0   <= (others=>'0');
				out_i1   <= (others=>'0');
				out_q0   <= (others=>'0');
				out_q1   <= (others=>'0');
			end if;
		end if;
	end process;
	
	-- connect output-clock
	out_clk <= oclk;
	
end rtl;


-------------------------------------------------------------------------------
-- halfband decimation filter - output control --------------------------------
-------------------------------------------------------------------------------

library ieee;
	use ieee.std_logic_1164.all;
	use ieee.numeric_std.all;
library work;
	use work.all;
	use work.mt_toolbox.all;
	use work.mt_filter.all;

entity usbrx_halfband_octrl is
	generic (
		N        : natural := 3
	);
	port (
		-- common
		clk      : in  std_logic;
		reset    : in  std_logic;
		
		-- input
		in_clk   : in  std_logic;
		in_chan  : in  unsigned(log2(2*N)-1 downto 0);
		in_data  : in  fir_dataword18;
		
		-- output
		out_clk  : out std_logic_vector(N-1 downto 0);
		out_i    : out fir_databus18(N-1 downto 0);
		out_q    : out fir_databus18(N-1 downto 0)
	);
end usbrx_halfband_octrl;

architecture rtl of usbrx_halfband_octrl is	

	-- cache
	signal tmp_i : fir_databus18(N-1 downto 0);

begin
	
	-- control logic
	process(clk)
		variable sel : natural range 0 to N-1;
	begin
		if rising_edge(clk) then
			-- set default values
			out_clk <= (others=>'0');
			
			-- handle input
			if in_clk='1' then
				sel := to_integer(in_chan)/2;
				for i in out_clk'range loop
					if sel=i then
						if in_chan(0)='0' then
							-- cache result
							tmp_i(i) <= in_data;
						else
							-- output result 
							out_clk(i) <= '1';
							out_i(i) <= tmp_i(i);
							out_q(i) <= in_data;
						end if;
					end if;
				end loop;
			end if;
			
			-- handle reset
			if reset='1' then
				tmp_i   <= (others=>(others=>'0'));
				out_clk <= (others=>'0');
				out_i   <= (others=>(others=>'0'));
				out_q   <= (others=>(others=>'0'));
			end if;
		end if;
	end process;
end rtl;


-------------------------------------------------------------------------------
-- halfband decimation filter -------------------------------------------------
-------------------------------------------------------------------------------

library ieee;
	use ieee.std_logic_1164.all;
	use ieee.numeric_std.all;
library work;
	use work.all;
	use work.mt_toolbox.all;
	use work.mt_filter.all;

entity usbrx_halfband is
	generic (
		N        : natural := 3
	);
	port (
		-- common
		clk      : in  std_logic;
		reset    : in  std_logic;
		
		-- input
		in_clk   : in  std_logic_vector(N-1 downto 0);
		in_i     : in  fir_databus18(N-1 downto 0);
		in_q     : in  fir_databus18(N-1 downto 0);
		
		-- output
		out_clk  : out std_logic_vector(N-1 downto 0);
		out_i    : out fir_databus18(N-1 downto 0);
		out_q    : out fir_databus18(N-1 downto 0)
	);
end usbrx_halfband;

architecture rtl of usbrx_halfband is	

	-- internal types
	type state_t is (S_IDLE, S_FILTER_I, S_FILTER_Q, S_COOLDOWN);
	
	-- status
	signal state : state_t;
	
	-- input control
	signal ic_ready : std_logic;
	signal ic_clk   : std_logic;
	signal ic_chan  : unsigned(log2(N)-1 downto 0);
	signal ic_i0    : fir_dataword18;
	signal ic_i1    : fir_dataword18;
	signal ic_q0    : fir_dataword18;
	signal ic_q1    : fir_dataword18;

	-- next output
	signal on_clk : std_logic;
	signal on_chan : unsigned(log2(2*N)-1 downto 0);
	signal on_dat1 : fir_dataword18;
	signal on_dat2 : fir_dataword18;
	
	-- output control
	signal oc_iclk  : std_logic;
	signal oc_ichan : unsigned(log2(2*N)-1 downto 0);
	signal oc_idata : fir_dataword18;
	
	-- FIR input
	signal fir_iclk  : std_logic;
	signal fir_iack  : std_logic;
	signal fir_idata : fir_dataword18;
	signal fir_ichan : unsigned(log2(2*N)-1 downto 0);
	
	-- FIR output
	signal fir_oclk  : std_logic;
	signal fir_ochan : unsigned(log2(2*N)-1 downto 0);
	signal fir_odata : fir_dataword18;
			
	-- center samples
	signal cent_i : fir_databus18(N-1 downto 0);
	signal cent_q : fir_databus18(N-1 downto 0);
	
	-- coefficients for 2x-FIR-interpolator
	-- (halfband, order 48, wpass 0.40)
	constant coeffs : fir_coefficients(0 to 11) := ( 
		  -52,    151,   -354,    715,
		-1307,   2227,  -3614,   5691,
		-8907,  14403, -26386,  82957
	);
	
begin

	-- control logic
	process(clk)
		variable sel : natural range 0 to N-1;
	begin
		if rising_edge(clk) then
			-- set default values
			fir_iclk <= '0';
			oc_iclk  <= '0';
			on_clk   <= '0';
		
			-- filter control
			case state is
				when S_IDLE =>
					-- wait for new sample-pair
					if ic_clk='1' then
						-- start filter (I)
						fir_iclk  <= '1';
						fir_idata <= ic_i1;
						fir_ichan <= resize(ic_chan & "0", fir_ichan'length);

						-- update status
						state <= S_FILTER_I;
					end if;
					
				when S_FILTER_I =>
					-- wait until filtering is done
					if fir_iack='1' then
						-- start filter (Q)
						fir_iclk  <= '1';
						fir_idata <= ic_q1;
						fir_ichan <= resize(ic_chan & "1", fir_ichan'length);
						
						-- update status
						state <= S_FILTER_Q;
					end if;
					
				when S_FILTER_Q =>
					-- wait until filtering is done
					if fir_iack='1' then
						-- update status
						state <= S_COOLDOWN;
					end if;
					
				when S_COOLDOWN =>
					-- TODO: get rid of state
					if fir_oclk='1' then
						state <= S_IDLE;
					end if;
			end case;
			
			-- fetch FIR result and corresonding center-sample
			if fir_oclk='1' then
				on_clk  <= '1';
				on_chan <= fir_ochan;
				on_dat1 <= fir_odata; 
				sel := to_integer(fir_ochan)/2;
				if fir_ochan(0)='0' 
					then on_dat2 <= cent_i(sel);
					else on_dat2 <= cent_q(sel);
				end if;
			end if;
			
			-- create final result
			if on_clk='1' then
				oc_iclk  <= '1';
				oc_ichan <= on_chan;
				oc_idata <= resize((resize(on_dat1,19) + resize(on_dat2,19)) / 2,18);
			end if;
			
			-- handle reset
			if reset='1' then
				state     <= S_IDLE;
				fir_iclk  <= '0';
				fir_idata <= (others=>'0');
				fir_ichan <= (others=>'0');
				oc_iclk   <= '0';
				oc_ichan  <= (others=>'0');
				oc_idata  <= (others=>'0');
				on_clk    <= '0';
				on_chan   <= (others=>'0');
				on_dat1   <= (others=>'0');
				on_dat2   <= (others=>'0');
			end if;
		end if;
	end process;
	
	-- create ready-flag
	ic_ready <= '1' when state=S_IDLE else '0';
	
	-- shift register for center-sample
	sreg: for i in 0 to N-1 generate
		subtype entry_t is signed(35 downto 0);
		type sreg_t is array(natural range<>) of entry_t;
		signal sreg : sreg_t(11 downto 0) := (others=>(others=>'0'));
	begin
		
		-- infer shift-register
		process(clk)
			variable temp : entry_t;
		begin
			if rising_edge(clk) then
				if ic_clk='1' and to_integer(ic_chan)=i then
					temp := ic_q0 & ic_i0;
					sreg <= sreg(sreg'left-1 downto 0) & temp;
				end if;
			end if;
		end process;
		
		-- output center-sample
		cent_i(i) <= sreg(sreg'left)(17 downto  0);
		cent_q(i) <= sreg(sreg'left)(35 downto 18);
	end generate;
	
	-- input control
	ic: entity usbrx_halfband_ictrl
		generic map (
			N        => N
		)
		port map (
			-- common
			clk      => clk,
			reset    => reset,
			
			-- input
			in_clk   => in_clk,
			in_i     => in_i,
			in_q     => in_q,
			
			-- output
			out_ready => ic_ready,
			out_clk   => ic_clk,
			out_chan  => ic_chan,
			out_i0    => ic_i0,
			out_i1    => ic_i1,
			out_q0    => ic_q0,
			out_q1    => ic_q1
		);

	-- FIR filter core
	fir: entity mt_fir_symmetric_slow
		generic map (
			CHANNELS  => 2*N,
			TAPS      => 12,
			COEFFS    => coeffs,
			RAMSTYLE  => "block_ram",
			ROMSTYLE  => "logic"
		)
		port map (
			-- common
			clk        => clk,
			reset      => reset,
			
			-- input port
			in_clk     => fir_iclk,
			in_ack     => fir_iack,
			in_data    => fir_idata,
			in_chan    => fir_ichan,
			
			-- output port
			out_clk    => fir_oclk,
			out_chan   => fir_ochan,
			out_data   => fir_odata
		);
	
	-- output control
	oc: entity usbrx_halfband_octrl
		generic map (
			N        => N
		)
		port map (
			-- common
			clk      => clk,
			reset    => reset,
			
			-- input
			in_clk   => oc_iclk,
			in_chan  => oc_ichan,
			in_data  => oc_idata,
			
			-- output
			out_clk  => out_clk,
			out_i    => out_i,
			out_q    => out_q
		);

end rtl;

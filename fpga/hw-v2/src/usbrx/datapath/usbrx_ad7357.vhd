---------------------------------------------------------------------------------------------------
-- Filename    : usbrx_ad7357.vhd
-- Project     : OsmoSDR FPGA Firmware
-- Purpose     : AD7357 Interface
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
library xp2;
	use xp2.all;
	use xp2.components.all;
library work;
	use work.all;
	use work.mt_toolbox.all;
	use work.usbrx.all;

entity usbrx_ad7357 is
	port(
		-- common
		clk        : in  std_logic;
		reset      : in  std_logic;

		-- config
		config     : in  usbrx_adc_config_t;
	
		-- ADC interface
		adc_cs     : out std_logic;
		adc_sck    : out std_logic;
		adc_sd1    : in  std_logic;
		adc_sd2    : in  std_logic;
		
		-- output
		out_clk    : out std_logic;
		out_i      : out unsigned(13 downto 0);
		out_q      : out unsigned(13 downto 0)
	);
end usbrx_ad7357;

architecture rtl of usbrx_ad7357 is

	-- internal types
	type state_t is (S_ACQUISITION, S_CONVERT);
	
	-- main status
	signal state   : state_t;
	signal counter : unsigned(7 downto 0);
	
	-- SCK generator
	signal cg_div    : unsigned(7 downto 0);
	signal cg_count  : unsigned(7 downto 0);
	signal cg_phase  : std_logic;
	signal cg_ddr_a  : std_logic;
	signal cg_ddr_b  : std_logic;
	signal cg_renxt  : std_logic;
	signal cg_redge  : std_logic;

	-- chip select (+ delayed versions)
	signal css : std_logic_vector(3 downto 0);
	
	-- latch input on rising/falling edge of current cycle
	-- (+ delayed versions)
	signal latch_r : std_logic_vector(3 downto 0);
	signal latch_f : std_logic_vector(3 downto 0);
	
	-- input register stage #1
	signal sd1_s1r : std_logic; -- SD1 - rising edge
	signal sd1_s1f : std_logic; -- SD1 - falling edge
	signal sd2_s1r : std_logic; -- SD2 - rising edge
	signal sd2_s1f : std_logic; -- SD2 - falling edge
	
	-- input register stage #2
	signal sd1_s2r : std_logic; -- SD1 - rising edge
	signal sd1_s2f : std_logic; -- SD1 - falling edge
	signal sd2_s2r : std_logic; -- SD2 - rising edge
	signal sd2_s2f : std_logic; -- SD2 - falling edge
	
	-- input shift registers
	signal sreg1 : std_logic_vector(13 downto 0);
	signal sreg2 : std_logic_vector(13 downto 0);
	
	-- output latches
	signal onew  : std_logic;
	signal oreg1 : std_logic_vector(13 downto 0);
	signal oreg2 : std_logic_vector(13 downto 0);

begin
	
	-- SCL clock generator logic
	process(clk)
	begin
		if rising_edge(clk) then
			-- set default values
			cg_renxt <= '0';
			cg_redge <= cg_renxt;
			latch_r <= latch_r(latch_r'left-1 downto 0) & '0';
			latch_f <= latch_f(latch_f'left-1 downto 0) & '0';
			
			-- get config
			cg_div <= config.clkdiv;
	
			-- get operation mode
			if cg_div=0 or cg_div=1 then
				--> full speed, just pass through clock
				cg_ddr_a   <= '0';
				cg_ddr_b   <= '1';
				cg_renxt   <= '1';
				latch_f(0) <= '1';
			else
				--> divided clock, update divider-logic
				if cg_count=0 then
					-- toggle clock on FE in middle of cycle 
					cg_count   <= cg_div - 2;
					cg_phase   <= not cg_phase;
					cg_ddr_a   <= cg_phase;
					cg_ddr_b   <= not cg_phase;
					cg_renxt   <= not cg_phase;
					latch_r(0) <= cg_phase;
				elsif cg_count=1 then
					-- toggle clock on RE after this cycle
					cg_count   <= cg_div - 1;
					cg_phase   <= '0'; --not cg_phase;
					cg_ddr_a   <= '1'; --cg_phase;
					cg_ddr_b   <= '1'; --cg_phase;
					latch_f(0) <= '1';
				else
					-- leave clock unchanged
					cg_count <= cg_count - 2;
					cg_ddr_a <= cg_phase;
					cg_ddr_b <= cg_phase;
				end if;
				
				-- failsafe
				if cg_count=1 and cg_div(0)='0' then
					cg_count <= cg_div - 2;
				end if;
				
			end if;
			
			-- handle reset
			if reset='1' then
				cg_div   <= (others=>'1');
				cg_count <= (others=>'0');
				cg_phase <= '0';
				cg_renxt <= '0';
				cg_redge <= '0';
				cg_ddr_a <= '1';
				cg_ddr_b <= '1';
				latch_r  <= (others=>'0');
				latch_f  <= (others=>'0');
			end if;
		end if;
	end process;
	
	-- output register for SCLK
	oddr: ODDRXC
		port map (
			clk => clk,
			rst => reset,
			da  => cg_ddr_a,
			db  => cg_ddr_b,
			q   => adc_sck
		);
	
	-- main control logig
	process(clk)
	begin
		if rising_edge(clk) then
			-- set default values
			css <= css(css'left-1 downto 0) & css(0);
				
			-- update status
			case state is
				when S_ACQUISITION =>
					-- doing ACQUISITION, wait until ready to start conversion
					if cg_redge='1' then
						-- new SCLK cycle, check if wait-counter has ellapsed
						counter <= counter - 1;
						if counter<=1 then
							--> counter ellapsed, start conversion
							state   <= S_CONVERT;
							counter <= to_unsigned(15,counter'length);
							css(0)  <= '0';
						end if;
					end if;
					
				when S_CONVERT =>
					-- doing conversion, wait until all bits are clocked out
					if cg_redge='1' then
						-- new SCLK cycle, check if bit-counter has ellapsed
						counter <= counter - 1;
						if counter=0 then
							-- all bits received, return to ACQUISITION state
							state   <= S_ACQUISITION;
							counter <= config.acqlen;
							css(0)  <= '1';
						end if;
					end if;
			end case;
			
			-- handle reset
			if reset='1' then
				state   <= S_ACQUISITION; -- TODO
				counter <= (others=>'0');
				css     <= (others=>'1');
			end if;
		end if;
	end process;
	
	-- output chip-select
	adc_cs <= css(0);

	-- input capture registers
	iddr1: IDDRXC
		port map (
			clk => clk,
			rst => '0',
			ce  => '1',
			d   => adc_sd1,
			qa  => sd1_s1r,
			qb  => sd1_s1f
		);
	iddr2: IDDRXC
		port map (
			clk => clk,
			rst => '0',
			ce  => '1',
			d   => adc_sd2,
			qa  => sd2_s1r,
			qb  => sd2_s1f
		);
	
	-- input data handling
	process(clk)
	begin
		if rising_edge(clk) then
			-- set default valies
			onew <= '0';
			
			-- register input-bits once more
			sd1_s2r <= sd1_s1r;
			sd1_s2f <= sd1_s1f;
			sd2_s2r <= sd2_s1r;
			sd2_s2f <= sd2_s1f;
			
			-- update shift-registers
			if latch_r(3)='1' then
				sreg1 <= sreg1(12 downto 0) & sd1_s2r;
				sreg2 <= sreg2(12 downto 0) & sd2_s2r;
			elsif latch_f(3)='1' then
				sreg1 <= sreg1(12 downto 0) & sd1_s2f;
				sreg2 <= sreg2(12 downto 0) & sd2_s2f;
			end if;
		
			-- latch away shift-register when requested
			if css(2)='1' and css(3)='0' then
				onew  <= '1';
				oreg1 <= sreg1;
				oreg2 <= sreg2;
			end if;
			
			-- handle reset
			if reset='1' then
				sd1_s2r <= '0';
				sd1_s2f <= '0';
				sd2_s2r <= '0';
				sd2_s2f <= '0';
				sreg1 <= (others=>'0');
				sreg2 <= (others=>'0');
				oreg1 <= (others=>'0');
				oreg2 <= (others=>'0');
				onew  <= '0';
			end if;
		end if;
	end process;
	
	-- set output
	out_clk <= onew;
	out_i <= unsigned(oreg1);
	out_q <= unsigned(oreg2);
		
end rtl;

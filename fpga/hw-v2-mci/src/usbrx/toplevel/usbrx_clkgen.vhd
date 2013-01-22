---------------------------------------------------------------------------------------------------
-- Filename    : usbrx_clkgen.vhd
-- Project     : OsmoSDR FPGA Firmware
-- Purpose     : Clock Management
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
	use ieee.std_logic_misc.all;
	use ieee.numeric_std.all;
library xp2;
	use xp2.all;
	use xp2.components.all;
library work;
	use work.all;
	use work.mt_toolbox.all;
	
entity usbrx_clkgen is
	port(
		-- clock input
		clk_30_pclk	: in  std_logic;		-- 30MHz
		ext_rst     : in  std_logic;		-- external reset
	
		-- system clock
		clk_30 		: out std_logic;		-- 30MHz clock
		clk_80 		: out std_logic;		-- 80MHz clock
		rst_30	 	: out std_logic;		-- 30MHz reset
		rst_80	 	: out std_logic			-- 80MHz reset
	);
end usbrx_clkgen;

architecture rtl of usbrx_clkgen is

	-- reset generation
	signal rst_pll 		: std_logic;		-- reset signal for PLLs
	signal pll_locked	: std_logic;		-- PLLs locked
	signal reset_i	 	: std_logic;		-- reset-signal
	
	-- system clock management
	signal clk_80_pll	: std_logic;
	
	-- enusure that signal-names are kept (important for timing contraints)
	attribute syn_keep of clk_80_pll : signal is true;
	
	-- component declaration
    component EPLLD1
        generic (CLKOK_BYPASS : in String; CLKOS_BYPASS : in String; 
                CLKOP_BYPASS : in String; DUTY : in Integer; 
                PHASEADJ : in String; PHASE_CNTL : in String; 
                CLKOK_DIV : in Integer; CLKFB_DIV : in Integer; 
                CLKOP_DIV : in Integer; CLKI_DIV : in Integer;
                FIN  : in String);
        port (CLKI: in std_logic; CLKFB: in std_logic; RST: in std_logic; 
            RSTK: in std_logic; DPAMODE: in std_logic; DRPAI3: in std_logic; 
            DRPAI2: in std_logic; DRPAI1: in std_logic; DRPAI0: in std_logic; 
            DFPAI3: in std_logic; DFPAI2: in std_logic; DFPAI1: in std_logic; 
            DFPAI0: in std_logic; PWD: in std_logic; CLKOP: out std_logic; 
            CLKOS: out std_logic; CLKOK: out std_logic; LOCK: out std_logic; 
            CLKINTFB: out std_logic);
    end component;
			
begin

	--
	-- reset generation
	--
	
	-- generate asynchronous reset-signal
	rg: entity mt_reset_gen
		port map (
			clk 		=> clk_30_pclk,
			ext_rst     => ext_rst,
			pll_locked	=> pll_locked,
			reset_pll	=> rst_pll,
			reset_sys	=> reset_i
		);
	
	-- sync reset to clock-domains
	rs30: entity mt_reset_sync
		port map (
			clk     => clk_30_pclk,
			rst_in  => reset_i, 
			rst_out => rst_30
		);
	rs80: entity mt_reset_sync
		port map (
			clk     => clk_80_pll,
			rst_in  => reset_i, 
			rst_out => rst_80
		);

	--
	-- system clock
	--
	
	-- PLL for system-clock
    pll : EPLLD1
		generic map (
			FIN          => "30.0",
			CLKOK_BYPASS => "DISABLED",
			CLKOS_BYPASS => "DISABLED", 
			CLKOP_BYPASS => "DISABLED", 
			PHASE_CNTL   => "STATIC", 
			DUTY         => 8, 
			PHASEADJ     => "0.0",
			CLKOK_DIV    => 2,
			CLKOP_DIV    => 8,
			CLKFB_DIV    => 8, 
			CLKI_DIV     => 3
		)
		port map (
			CLKI    => clk_30_pclk,
			CLKFB   => clk_80_pll,
			RST     => rst_pll, 
			RSTK    => '0', 
			DPAMODE => '0',
			DRPAI3  => '0', 
			DRPAI2  => '0',
			DRPAI1  => '0',
			DRPAI0  => '0', 
			DFPAI3  => '0',
			DFPAI2  => '0',
			DFPAI1  => '0', 
			DFPAI0  => '0',
			PWD     => '0',
			CLKOP   => clk_80_pll, 
			CLKOS   => open, 
			CLKOK   => open,
			LOCK    => pll_locked,
			CLKINTFB=> open
		);
			
	-- output clocks
	clk_30 <= clk_30_pclk;
	clk_80 <= clk_80_pll;
	
end rtl;

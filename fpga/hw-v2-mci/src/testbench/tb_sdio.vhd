library ieee;
	use ieee.std_logic_1164.all;
	use ieee.std_logic_misc.all;
	use ieee.numeric_std.all;
	use ieee.math_real.all;
library work;
	use work.all;
	use work.mt_toolbox.all;
	
entity tb_sdio is
end tb_sdio;

architecture rtl of tb_sdio is

	--
	-- signals
	--
	
	-- system clock
	signal clk_sys : std_logic := '1';
	signal rst_sys : std_logic := '1';

	-- SDIO lines
	signal sd_clk : std_logic := '1';
	signal sd_cmd : std_logic;
	signal sd_dat : std_logic_vector(3 downto 0);
	
	-- input samples
	signal smp_clk : std_logic;
	signal smp_raw : std_logic_vector(31 downto 0);
	signal smp_i   : signed(15 downto 0);
	signal smp_q   : signed(15 downto 0);
	
	--
	-- procedures
	--
	
	-- CRC7
	procedure crc7_update(
		crc: inout std_logic_vector(6 downto 0);
		dat: in    std_logic) is
	begin
		if dat /= crc(6)
			then crc := (crc(5 downto 0) & "0") xor "0001001";
			else crc := (crc(5 downto 0) & "0");
		end if;
	end procedure;
	procedure crc7_update(
		crc: inout std_logic_vector(6 downto 0);
		dat: in    std_logic_vector) is
	begin
		for i in dat'range loop
			crc7_update(crc,dat(i));
		end loop;
	end procedure;
	procedure crc7_compute(
		crc: out std_logic_vector(6 downto 0);
		dat: in  std_logic_vector) 
	is
		variable tmp : std_logic_vector(6 downto 0) := (others=>'0');
	begin
		crc7_update(tmp,dat);
		crc := tmp;
	end procedure;
	
	-- CRC16
	procedure crc16_update(
		crc: inout std_logic_vector(15 downto 0);
		dat: in    std_logic) is
	begin
		if dat /= crc(15)
			then crc := (crc(14 downto 0) & "0") xor x"1021";
			else crc := (crc(14 downto 0) & "0");
		end if;
	end procedure;
	procedure crc16_update(
		crc: inout std_logic_vector(15 downto 0);
		dat: in    std_logic_vector) is
	begin
		for i in dat'range loop
			crc16_update(crc,dat(i));
		end loop;
	end procedure;
	
begin
	
	-- generate system-clock
	clk_sys <= not clk_sys after 500ns / 80.0;
	rst_sys <= '1', '0' after 100ns;
	
	-- generate SD-clock
	sd_clk <= not sd_clk after 500ns / 25.0;
	
	-- pull-ups
	sd_cmd <= 'H'; 
	sd_dat <= "HHHH";
	
	-- test #1
	process
	
		--
		-- low-level helper
		--
		
		-- send command
		procedure send_command (
			cmd : natural;
			arg : std_logic_vector(31 downto 0)
		) is
			variable msg : std_logic_vector(47 downto 0);
		begin
			-- compose message
			msg(47) := '0';
			msg(46) := '1';
			msg(45 downto 40) := std_logic_vector(to_unsigned(cmd,6));
			msg(39 downto  8) := arg;
			crc7_compute(msg(7 downto 1),msg(47 downto 8));
			msg(0) := '1';
			
			-- transmit message
			for i in 47 downto 0 loop
				wait until falling_edge(sd_clk);
				sd_cmd <= msg(i);
			end loop;
			
			-- release bus
			wait until falling_edge(sd_clk);
			sd_cmd <= 'Z';
		end procedure;
	
		-- receive response
		procedure receive_response (
			cmd : out natural;
			arg : out std_logic_vector(31 downto 0);
			err : out std_logic
		) is
			variable msg : std_logic_vector(46 downto 0);
			variable crc1 : std_logic_vector(6 downto 0);
			variable crc2 : std_logic_vector(6 downto 0);
		begin
			-- wait for start-bit
			wait until rising_edge(sd_clk) and sd_cmd='0';
			
			-- receive message
			for i in 46 downto 0 loop
				wait until rising_edge(sd_clk);
				msg(i) := sd_cmd;
			end loop;
			
			-- decode message
			cmd  := to_integer(unsigned(msg(45 downto 40)));
			arg  := msg(39 downto 8);
			crc1 := msg(7 downto 1);
			
			-- verify CRC
			crc7_compute(crc2,msg(46 downto 8));
			if crc1=crc2
				then err := '0';
				else err := '1';
			end if;
		end procedure;
		
		--
		-- mid-level helper
		--
		
		-- send command #52
		procedure send_cmd52 (
			rw  : std_logic;
			fun : natural;
			raw : std_logic;
			adr : natural;
			dat : std_logic_vector(7 downto 0)
		) is
			variable arg : std_logic_vector(31 downto 0);
		begin
			-- compose argument
			arg(31) := rw;
			arg(30 downto 28) := std_logic_vector(to_unsigned(fun,3));
			arg(27) := raw;
			arg(26) := '1';
			arg(25 downto 9) := std_logic_vector(to_unsigned(adr,17));
			arg(8) := '1';
			arg(7 downto 0) := dat;
			
			-- send command
			send_command(52,arg);
		end procedure;
		
		-- send command #53
		procedure send_cmd53 (
			rw  : std_logic;
			fun : natural;
			blk : std_logic;
			opc : std_logic;
			adr : natural;
			cnt : natural
		) is
			variable arg : std_logic_vector(31 downto 0);
		begin
			-- compose argument
			arg(31) := rw;
			arg(30 downto 28) := std_logic_vector(to_unsigned(fun,3));
			arg(27) := blk;
			arg(26) := opc;
			arg(25 downto 9) := std_logic_vector(to_unsigned(adr,17));
			arg(8 downto 0) := std_logic_vector(to_unsigned(cnt,9));
			
			-- send command
			send_command(53,arg);
		end procedure;
		
		
		--
		-- high-level helper
		--
			
		-- write single register
		procedure write_reg (
			adr  : in  natural;
			idat : in  std_logic_vector(7 downto 0);
			odat : out std_logic_vector(7 downto 0);
			stat : out std_logic_vector(7 downto 0);
			err  : out std_logic
		) is
			variable res_cmd : natural;
			variable res_arg : std_logic_vector(31 downto 0);
			variable res_err : std_logic;
		begin
			-- send command
			send_cmd52('1',0,'0',adr,idat);
			
			-- get response
			receive_response(res_cmd,res_arg,res_err);
			if res_err='1' or res_cmd/=52 then
				err  := '1';
				odat := x"00";
				stat := x"00";
			else
				err  := '0';
				stat := res_arg(15 downto 8);
				odat := res_arg( 7 downto 0);
			end if;
		end procedure;
		
		-- read single register
		procedure read_reg (
			adr  : in  natural;
			odat : out std_logic_vector(7 downto 0);
			stat : out std_logic_vector(7 downto 0);
			err  : out std_logic
		) is
			variable res_cmd : natural;
			variable res_arg : std_logic_vector(31 downto 0);
			variable res_err : std_logic;
		begin
			-- send command
			send_cmd52('0',0,'0',adr,x"00");
			
			-- get response
			receive_response(res_cmd,res_arg,res_err);
			if res_err='1' or res_cmd/=52 then
				err  := '1';
				odat := x"00";
				stat := x"00";
			else
				err  := '0';
				stat := res_arg(15 downto 8);
				odat := res_arg( 7 downto 0);
			end if;
		end procedure;
	
		-- helper vars
		variable my_odat : std_logic_vector(7 downto 0);
		variable my_stat : std_logic_vector(7 downto 0);
		variable my_err  : std_logic;
		
	begin
		
		-- init signals
		sd_cmd <= 'Z'; 
		sd_dat <= "ZZZZ";
		wait for 200ns;
		
		-- request data-stream
		send_cmd53('0',1,'1','0',1234,0);
		
		-- wait some time
		wait for 2960us;
		
		-- abort data-stream
		write_reg(6,x"01",my_odat,my_stat,my_err);
		
		
		-- TODO
--		read_reg(1,my_odat,my_stat,my_err);
		
		wait;
	end process;
	
	-- test #2
	process
		variable msg     : slv8_array_t(0 to 511);
		variable crc_rx  : slv16_array_t(0 to 3);
		variable crc_ref : slv16_array_t(0 to 3);
		variable blk_ok  : std_logic := '0';
		variable blk_err : std_logic := '0';
	begin
		loop
			-- wait for start-bit
			wait until rising_edge(sd_clk) and sd_dat(0)='0';
			
			-- reset CRCs
			crc_ref := (others=>x"0000");
			
			-- receive message
			for i in 0 to 511 loop
				wait until rising_edge(sd_clk);
				msg(i)(7 downto 4) := sd_dat;
				crc16_update(crc_ref(0), sd_dat(0));
				crc16_update(crc_ref(1), sd_dat(1));
				crc16_update(crc_ref(2), sd_dat(2));
				crc16_update(crc_ref(3), sd_dat(3));
				
				wait until rising_edge(sd_clk);
				msg(i)(3 downto 0) := sd_dat;
				crc16_update(crc_ref(0), sd_dat(0));
				crc16_update(crc_ref(1), sd_dat(1));
				crc16_update(crc_ref(2), sd_dat(2));
				crc16_update(crc_ref(3), sd_dat(3));
			end loop;
			
			-- receive CRCs
			for i in 15 downto 0 loop
				wait until rising_edge(sd_clk);
				crc_rx(0)(i) := sd_dat(0);
				crc_rx(1)(i) := sd_dat(1);
				crc_rx(2)(i) := sd_dat(2);
				crc_rx(3)(i) := sd_dat(3);
			end loop;
			
			-- check CRCs
			if crc_rx=crc_ref
				then blk_ok  := '1';
				else blk_err := '1';
			end if;
			wait until rising_edge(sd_clk);
			blk_ok  := '0';
			blk_err := '0';
		end loop;
		wait;
	end process;
	
	-- input control
	process
		variable counter1 : signed(15 downto 0) := (others=>'0');
		variable counter2 : signed(15 downto 0) := (others=>'0');
	begin
		smp_clk  <= '0';
		smp_i    <= to_signed(0,16);
		smp_q    <= to_signed(0,16);
		wait until rising_edge(clk_sys) and rst_sys='0';
		
		loop
			-- wait some time
			for i in 0 to 78 loop
				wait until rising_edge(clk_sys);
			end loop;
			
			-- update counter
			counter1 := counter1 + 1;
			counter2 := counter2 - 1;
			
			-- get sample data
			smp_i <= counter1;
			smp_q <= counter2;

			-- input sample
			smp_clk <= '1';
			wait until rising_edge(clk_sys);
			smp_clk <= '0';
		end loop;
		
		wait;
	end process;
	
	-- pack input into 32 bit raw value
	smp_raw <= std_logic_vector(smp_q & smp_i);
	
	-- unit under test #1
	uut1: entity sdio_top
		port map (
			-- common
			clk     => clk_sys,
			reset   => rst_sys,
			
			-- data input
			in_clk  => smp_clk,
			in_data => smp_raw,
	
			-- SDIO interface
			sd_clk  => sd_clk,
			sd_cmd  => sd_cmd,
			sd_dat  => sd_dat
		);

end rtl;



-- The control plane, wrapped for VHDL and no connecting logic
library IEEE; use IEEE.std_logic_1164.all, IEEE.numeric_std.all, std.textio.all;
library ocpi; use ocpi.all, ocpi.types.all;
library platform; use platform.platform_pkg.all, platform.metadata_defs.all;
library bsv;
entity metadata_rv is
  port(
    metadata_in  : in  metadata_out_t;
    metadata_out : out metadata_in_t
    );
end entity metadata_rv;
architecture rtl of metadata_rv is
  component mkUUID is
    port (uuid : out std_logic_vector(511 downto 0));
  end component mkUUID;
  signal myUUID : std_logic_vector(511 downto 0);

  type meta_t is array (0 to 1023) of std_logic_vector(31 downto 0);
  impure function initmeta return meta_t is
    file metafile : text open read_mode is "metadatarom.dat";
    variable metamem : meta_t;
    variable metaline : line;
    variable metabits : bit_vector(31 downto 0);
  begin
    for i in meta_t'range loop
      readline(metafile, metaline);
      read(metaline, metabits);
      metamem(i) := to_stdlogicvector(metabits);
    end loop;
    return metamem;
  end function;

  constant metamem : meta_t := initmeta;
begin
  uu : mkUUID
  port map(
    uuid => myUUID
    );
  gen0: for i in 0 to metadata_out.UUID'right generate
    metadata_out.UUID(i) <= to_ulong(myUUID(511 - i*32 downto 511 - i*32 - 31));
  end generate gen0;

  process (metadata_in.clk)
  begin
    if rising_edge(metadata_in.clk) then
      metadata_out.romData <= to_ulong(metamem(to_integer(metadata_in.romAddr(9 downto 0))));
    end if;
  end process;
  
end architecture rtl;
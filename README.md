WARNING
=======

The `hddtemp` sensor is now integrated into gkrellm2, and will appear among other sensors as soon as you'll have properly setup the `hddtemp` daemon as explain in the following part.

### Installation
Just type `make gkrellm2 && make install2` to use with current gkrellm2.
Respective `gkrellm1` and `install1` makefile targets are provided for legacy purpose.

Before starting `gkrellm`, you must start `hddtemp` (version >= 0.3) in daemon mode.
This is automatically handled in Debian stable (Jessie) if `hddtemp` is intalled and you edit the following in `/etc/default/hddtemp`:

`RUN_DAEMON="true"`

For the moment gkrellm-hddtemp isn't able to connect to a distant host.

### Contact
- Branch maintainer : Simon Descarpentries <sdescarpentries à april.org>
- Original developper : Emmanuel Varagnat <coredump à free.fr>,  <emmanuel.varagnat à free.fr>

### Licence
Software under GLPv2 licence. See LICENCE file for more information.

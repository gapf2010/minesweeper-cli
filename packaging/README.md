# Packaging minesweeper-cli

This folder has what you need to build packages for Fedora, Ubuntu (and
Debian), Arch Linux, Slackware and OpenBSD. Every package installs:

* the games `mine-classic` and `mine-path`
* their man page, section 6 (`man mine-classic`, `man mine-path`)
* the `LICENSE` (MIT) and `README.md`

Scores are per user, in `~/.config/mine-cli/`, so packages install nothing
there.

## 1. Make a release first

Every package downloads the source of a tagged release from GitHub, so
publish the tag before building:

```
git tag -a v0.2.0 -m "minesweeper-cli 0.2.0"
git push origin v0.2.0
```

GitHub then serves the source at
`https://github.com/gapf2010/minesweeper-cli/archive/v0.2.0/minesweeper-cli-0.2.0.tar.gz`.

For a later release, change the version in `common.h` (`MS_VERSION`), in
the man page header (`man/mine-classic.6`) and in each packaging file
(listed under "New versions" at the end), then tag again.

## 2. Fedora (RPM)

Files: `fedora/minesweeper-cli.spec`

```
sudo dnf install rpm-build rpmdevtools gcc make
rpmdev-setuptree
cp packaging/fedora/minesweeper-cli.spec ~/rpmbuild/SPECS/
spectool -g -R ~/rpmbuild/SPECS/minesweeper-cli.spec
rpmbuild -ba ~/rpmbuild/SPECS/minesweeper-cli.spec
sudo dnf install ~/rpmbuild/RPMS/*/minesweeper-cli-0.2.0-1.*.rpm
```

`spectool` downloads the release; `rpmbuild` builds with Fedora's
hardening flags, runs the self-tests and writes the binary RPM under
`~/rpmbuild/RPMS/` and the source RPM under `~/rpmbuild/SRPMS/`.
Optionally check it with `rpmlint`.

## 3. Ubuntu and Debian (.deb)

Files: `debian/` (control, rules, changelog, copyright, source/format)

Debian tools expect the `debian/` folder at the top of the source tree,
so copy it there inside the release source:

```
sudo apt install build-essential debhelper wget
wget -O minesweeper-cli_0.2.0.orig.tar.gz \
  https://github.com/gapf2010/minesweeper-cli/archive/v0.2.0/minesweeper-cli-0.2.0.tar.gz
tar xf minesweeper-cli_0.2.0.orig.tar.gz
cd minesweeper-cli-0.2.0
cp -r packaging/debian debian
dpkg-buildpackage -us -uc
sudo apt install ../minesweeper-cli_0.2.0-1_*.deb
```

The games install into `/usr/games`, as Debian policy asks; Ubuntu and
Debian have it on the default `PATH`. Optionally check the result with
`lintian ../minesweeper-cli_0.2.0-1_*.changes`.

To publish on a Launchpad PPA, change `unstable` in `debian/changelog`
to your Ubuntu release name (for example `noble`) and sign the upload.

## 4. Arch Linux

Files: `arch/PKGBUILD`

```
sudo pacman -S --needed base-devel pacman-contrib
mkdir build && cp packaging/arch/PKGBUILD build/ && cd build
updpkgsums
makepkg -si
```

`updpkgsums` (from `pacman-contrib`) replaces the `SKIP` checksum with
the real one for the release; keep that in the PKGBUILD if you publish
it on the AUR (where you also need `makepkg --printsrcinfo > .SRCINFO`).
`makepkg -si` builds, runs the self-tests, and installs.

## 5. Slackware

Files: `slackware/` (SlackBuild, slack-desc, .info, README), in the
SlackBuilds.org layout.

As root:

```
cd packaging/slackware
wget https://github.com/gapf2010/minesweeper-cli/archive/v0.2.0/minesweeper-cli-0.2.0.tar.gz
sh minesweeper-cli.SlackBuild
installpkg /tmp/minesweeper-cli-0.2.0-*-1_SBo.tgz
```

The games install into `/usr/games` and the man pages into `/usr/man`,
as is usual on Slackware. To submit to SlackBuilds.org, put the
output of `md5sum minesweeper-cli-0.2.0.tar.gz` into `MD5SUM` in
`minesweeper-cli.info`.

## 6. OpenBSD

Files: `openbsd/games/minesweeper-cli/`, laid out as a port.

With the ports tree in `/usr/ports` (see the OpenBSD FAQ on ports):

```
cp -R packaging/openbsd/games/minesweeper-cli /usr/ports/mystuff/games/
cd /usr/ports/mystuff/games/minesweeper-cli
make makesum
make test
make install
```

`make makesum` downloads the release and creates `distinfo` with its
checksum; `make test` builds and runs the self-tests; `make install`
builds the package and installs it with `pkg_add`. The package itself
ends up in `/usr/ports/packages/$(arch -s)/all/`. If you change what gets
installed, `make update-plist` refreshes `pkg/PLIST`.

## Installing without a package

```
make
sudo make install            # into /usr/local
sudo make uninstall
```

`PREFIX`, `BINDIR`, `MANDIR` and `DESTDIR` can all be overridden, for
example `make install PREFIX=$HOME/.local`. On OpenBSD add
`MANDIR=/usr/local/man`.

## New versions

When releasing a new version, update it in:

* `common.h` (`MS_VERSION`) and `man/mine-classic.6` (first line)
* `fedora/minesweeper-cli.spec`: `Version`, reset `Release` to 1, add a
  `%changelog` entry
* `debian/changelog`: add an entry on top (`dch -v X.Y-1` does it)
* `arch/PKGBUILD`: `pkgver`, reset `pkgrel` to 1, run `updpkgsums`
* `slackware/minesweeper-cli.SlackBuild` (`VERSION`) and
  `slackware/minesweeper-cli.info` (`VERSION`, `DOWNLOAD`, `MD5SUM`)
* `openbsd/games/minesweeper-cli/Makefile` (`GH_TAGNAME`), then
  `make makesum`

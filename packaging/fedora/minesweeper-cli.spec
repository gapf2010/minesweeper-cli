Name:           minesweeper-cli
Version:        0.2.0
Release:        1%{?dist}
Summary:        Minesweeper games for the terminal: classic and pathfinder

License:        MIT
URL:            https://github.com/gapf2010/minesweeper-cli
Source0:        %{url}/archive/v%{version}/%{name}-%{version}.tar.gz

BuildRequires:  gcc
BuildRequires:  make

%description
Two minesweeper games that run in a text terminal. mine-classic is the
classic game, with a safe first dig, chording, flags and question marks.
mine-path is the pathfinder game: walk from one corner of a hidden
minefield to the other, scored on time and on how much you explored.
Both have a live timer, a pause key, a daily challenge, replayable seeds
and a top-5 score table per difficulty.

%prep
%autosetup

%build
%set_build_flags
%make_build CFLAGS="%{build_cflags}" LDFLAGS="%{build_ldflags}"

%install
%make_install PREFIX=%{_prefix} BINDIR=%{_bindir} MANDIR=%{_mandir}

%check
make test

%files
%license LICENSE
%doc README.md
%{_bindir}/mine-classic
%{_bindir}/mine-path
%{_mandir}/man6/mine-classic.6*
%{_mandir}/man6/mine-path.6*

%changelog
* Thu Nov 26 2026 German Pulido <1203374+gapf2010@users.noreply.github.com> - 0.2.0-1
- Initial package

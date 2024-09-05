# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.2] - 2024-09-05

### Added

- `co::set_active_as_main()` for better interoperability with codebases that already use `libco`.

## [0.1.1] - 2024-09-03

### Added

- CHANGELOG.MD

### Changed

- `cppco` now only requires C++11
- Cleaned up includes

### Removed

## [0.1.0] - 2024-08-27

### Added

- Initial release of `cppco` - a header-only C++ wrapper for `libco`
- External CMakeLists.txt for `libco`
- Tests using GTest and GMock
- README.MD
- ISC License
- CI via GitHub actions

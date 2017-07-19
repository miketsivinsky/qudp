TEMPLATE = subdirs

include(build.pri)

SUBDIRS = \
            src     \
            test_rx \
            test_tx

src.subdir     = src
test_rx.subdir = test/test_rx
test_tx.subdir = test/test_tx

test_rx.depends = src
test_tx.depends = src

CONFIG += ordered


import sys, os


def test_preimported_modules():
    lst = ['__builtin__', '_codecs', '_warnings', 'codecs', 'encodings',
           'exceptions', 'signal', 'sys', 'zipimport']
    g = os.popen('"%s" -c "import sys; print sorted(sys.modules)"' %
                 (sys.executable,))
    real_data = g.read()
    g.close()
    for name in lst:
        quoted_name = repr(name)
        assert quoted_name in real_data

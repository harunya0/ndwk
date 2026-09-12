package ndwk;

public enum NdwkLang {
    JA(0),
    ZH(1),
    KO(2),
    EN(3),
    OMNI(4);

    private final int value;

    NdwkLang(int value) {
        this.value = value;
    }

    public int getValue() {
        return value;
    }

    public static NdwkLang fromValue(int val) {
        for (var lang : values()) {
            if (lang.value == val) return lang;
        }
        return JA;
    }
}

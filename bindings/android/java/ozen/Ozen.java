// SPDX-License-Identifier: MIT OR Apache-2.0
package ozen;

import java.nio.charset.StandardCharsets;

public final class Ozen {
  static {
    System.loadLibrary("ozen_jni");
  }

  private Ozen() {}

  public static native byte[] buildUtf8(
      byte[][] paths, byte[][] texts, byte[] kitchen, byte[] options, byte[] entry, int emit);

  public static String build(
      String[] paths, String[] texts, String kitchen, String options, String entry, int emit) {
    byte[][] p = new byte[paths.length][];
    byte[][] t = new byte[texts.length][];
    for (int i = 0; i < paths.length; i++) {
      p[i] = paths[i].getBytes(StandardCharsets.UTF_8);
      t[i] = texts[i].getBytes(StandardCharsets.UTF_8);
    }
    byte[] out = buildUtf8(p, t, utf8(kitchen), utf8(options), utf8(entry), emit);
    return out == null ? null : new String(out, StandardCharsets.UTF_8);
  }

  private static byte[] utf8(String s) {
    return s == null ? null : s.getBytes(StandardCharsets.UTF_8);
  }
}

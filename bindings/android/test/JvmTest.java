// SPDX-License-Identifier: MIT OR Apache-2.0
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import ozen.Ozen;

public final class JvmTest {
  public static void main(String[] args) throws Exception {
    Path out = Paths.get(args[0]);
    for (int i = 1; i < args.length; i++) {
      Path input = Paths.get(args[i]);
      String text = new String(Files.readAllBytes(input), StandardCharsets.UTF_8);
      String name = input.getFileName().toString();
      String result =
          Ozen.build(new String[] {name}, new String[] {text}, null, null, null, 1 | 2 | 4 | 16);
      Files.write(out.resolve((i - 1) + ".json"), (result == null ? "" : result).getBytes(StandardCharsets.UTF_8));
    }
  }
}

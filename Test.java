import java.io.*;

public class Test {
    public static void main(String[] args) {
        try {
            if (!checkNbfcStatus()) {
                System.out.println("NBFC chưa chạy, đang khởi động...");
                if (startNbfcWithPassword()) {
                    System.out.println("NBFC đã được khởi động.");
                    checkNbfcStatus(); // Kiểm tra lại
                } else {
                    System.out.println("Không thể khởi động NBFC.");
                }
            }
        } catch (Exception e) {
            System.out.println("Lỗi: " + e.getMessage());
        }
    }

    // Hàm kiểm tra trạng thái NBFC
    public static boolean checkNbfcStatus() {
        try {
            ProcessBuilder builder = new ProcessBuilder("nbfc", "status");
            Process process = builder.start();

            BufferedReader reader = new BufferedReader(
                    new InputStreamReader(process.getInputStream())
            );

            String line;
            boolean hasOutput = false;
            while ((line = reader.readLine()) != null) {
                hasOutput = true;
                System.out.println(line);
            }

            int exitCode = process.waitFor();
            return (exitCode == 0) && hasOutput;
        } catch (Exception e) {
            return false;
        }
    }

    // Hàm khởi động NBFC với mật khẩu sudo
    public static boolean startNbfcWithPassword() {
        try {
            Console console = System.console();
            if (console == null) {
                System.out.println("Không thể lấy Console. Vui lòng chạy bằng terminal.");
                return false;
            }

            char[] passwordArray = console.readPassword("Nhập mật khẩu sudo: ");
            String password = new String(passwordArray);

            ProcessBuilder builder = new ProcessBuilder("sudo", "-S", "nbfc", "start");
            Process process = builder.start();

            BufferedWriter writer = new BufferedWriter(
                    new OutputStreamWriter(process.getOutputStream()));
            writer.write(password + "\n");
            writer.flush();

            BufferedReader errorReader = new BufferedReader(
                    new InputStreamReader(process.getErrorStream()));
            String line;
            boolean success = true;
            while ((line = errorReader.readLine()) != null) {
                System.err.println(line);
                if (line.contains("incorrect password")) {
                    success = false;
                }
            }

            int exitCode = process.waitFor();
            return success && exitCode == 0;

        } catch (Exception e) {
            System.out.println("Lỗi khi chạy sudo: " + e.getMessage());
            return false;
        }
    }
}

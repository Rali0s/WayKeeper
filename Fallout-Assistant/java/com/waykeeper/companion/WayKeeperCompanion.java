package com.waykeeper.companion;

import java.awt.BasicStroke;
import java.awt.Color;
import java.awt.Dimension;
import java.awt.Font;
import java.awt.Graphics;
import java.awt.Graphics2D;
import java.awt.GraphicsEnvironment;
import java.awt.Rectangle;
import java.awt.RenderingHints;
import java.awt.Toolkit;
import java.awt.event.WindowAdapter;
import java.awt.event.WindowEvent;
import java.awt.image.BufferedImage;
import java.io.IOException;
import java.nio.channels.FileChannel;
import java.nio.channels.FileLock;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.util.HashMap;
import java.util.Map;
import java.util.Optional;
import java.util.Timer;
import java.util.TimerTask;
import javax.imageio.ImageIO;
import javax.swing.JFrame;
import javax.swing.JPanel;
import javax.swing.SwingUtilities;

/** Desktop-only full-resolution companion. The ANSI portrait remains the embedded fallback. */
public final class WayKeeperCompanion {
    private static final Color GOLD = new Color(255, 191, 0);
    private static final Color GREEN = new Color(75, 255, 110);
    private static final Color PANEL = new Color(9, 13, 10);
    private static FileChannel lockChannel;
    private static FileLock processLock;

    private WayKeeperCompanion() {}

    public static void main(String[] arguments) {
        Map<String, String> options = parseArguments(arguments);
        Path root = Path.of(options.getOrDefault("root", ".")).toAbsolutePath().normalize();
        Path profile = Path.of(options.getOrDefault("profile", root.resolve("state/profile.ini").toString()));
        Path settings = Path.of(options.getOrDefault("settings", root.resolve("state/settings.ini").toString()));
        long parent = parseLong(options.get("parent"), -1L);

        if (options.containsKey("snapshot")) {
            renderSnapshot(root, profile, Path.of(options.get("snapshot")));
            return;
        }

        if (GraphicsEnvironment.isHeadless() || !companionEnabled(settings) || !acquireLock(settings)) {
            return;
        }
        SwingUtilities.invokeLater(() -> createWindow(root, profile, settings, parent));
    }

    private static void renderSnapshot(Path root, Path profile, Path output) {
        CompanionPanel panel = new CompanionPanel(root, profile);
        panel.setSize(420, 560);
        BufferedImage snapshot = new BufferedImage(420, 560, BufferedImage.TYPE_INT_ARGB);
        Graphics2D graphics = snapshot.createGraphics();
        panel.paint(graphics);
        graphics.dispose();
        try {
            ImageIO.write(snapshot, "png", output.toFile());
        } catch (IOException exception) {
            throw new IllegalStateException("Could not write companion snapshot", exception);
        }
    }

    private static void createWindow(Path root, Path profile, Path settings, long parent) {
        CompanionPanel panel = new CompanionPanel(root, profile);
        JFrame frame = new JFrame("WayKeeper (TM) // Full-Fidelity Companion");
        frame.setDefaultCloseOperation(JFrame.DISPOSE_ON_CLOSE);
        frame.setBackground(PANEL);
        frame.setContentPane(panel);
        frame.setMinimumSize(new Dimension(300, 410));
        frame.setSize(420, 560);
        frame.setResizable(true);
        frame.setAlwaysOnTop(false);
        frame.addWindowListener(new WindowAdapter() {
            @Override public void windowClosed(WindowEvent event) {
                releaseLock();
            }
        });

        Rectangle desktop = GraphicsEnvironment.getLocalGraphicsEnvironment().getMaximumWindowBounds();
        int x = desktop.x + Math.max(0, desktop.width - frame.getWidth() - 18);
        int y = desktop.y + 18;
        frame.setLocation(x, y);
        frame.setVisible(true);

        Timer monitor = new Timer("waykeeper-companion-monitor", true);
        monitor.scheduleAtFixedRate(new TimerTask() {
            @Override public void run() {
                boolean parentAlive = parent <= 0 || ProcessHandle.of(parent)
                    .map(ProcessHandle::isAlive).orElse(false);
                if (!parentAlive || !companionEnabled(settings)) {
                    SwingUtilities.invokeLater(frame::dispose);
                    cancel();
                    return;
                }
                SwingUtilities.invokeLater(panel::refreshProfile);
            }
        }, 700L, 700L);
    }

    private static final class CompanionPanel extends JPanel {
        private final Path root;
        private final Path profile;
        private String incident = "General Survival";
        private String operator = "OPERATOR";
        private String imageName = "";
        private BufferedImage image;

        CompanionPanel(Path root, Path profile) {
            this.root = root;
            this.profile = profile;
            setBackground(PANEL);
            setDoubleBuffered(true);
            refreshProfile();
        }

        void refreshProfile() {
            Map<String, String> values = readIni(profile);
            String nextIncident = values.getOrDefault("incident", "General Survival");
            String nextOperator = values.getOrDefault("name", "OPERATOR");
            String nextImage = imageForIncident(nextIncident);
            operator = nextOperator;
            incident = nextIncident;
            if (!nextImage.equals(imageName)) {
                imageName = nextImage;
                image = loadImage(root, imageName);
            }
            repaint();
        }

        @Override protected void paintComponent(Graphics graphics) {
            super.paintComponent(graphics);
            Graphics2D g = (Graphics2D)graphics.create();
            g.setRenderingHint(RenderingHints.KEY_RENDERING, RenderingHints.VALUE_RENDER_QUALITY);
            g.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);
            g.setRenderingHint(RenderingHints.KEY_INTERPOLATION, RenderingHints.VALUE_INTERPOLATION_BICUBIC);
            g.setRenderingHint(RenderingHints.KEY_ALPHA_INTERPOLATION, RenderingHints.VALUE_ALPHA_INTERPOLATION_QUALITY);
            g.setRenderingHint(RenderingHints.KEY_COLOR_RENDERING, RenderingHints.VALUE_COLOR_RENDER_QUALITY);

            int width = getWidth();
            int height = getHeight();
            g.setColor(new Color(13, 20, 14));
            for (int x = 0; x < width; x += 24) g.drawLine(x, 0, x, height);
            for (int y = 0; y < height; y += 24) g.drawLine(0, y, width, y);

            g.setStroke(new BasicStroke(2.0f));
            g.setColor(GOLD);
            g.drawRect(8, 8, Math.max(0, width - 17), Math.max(0, height - 17));
            g.setFont(new Font(Font.MONOSPACED, Font.BOLD, 17));
            g.drawString("WAYKEEPER", 20, 34);
            g.setFont(new Font(Font.MONOSPACED, Font.PLAIN, 11));
            g.setColor(GREEN);
            g.drawString(modeLabel(incident), 20, 52);

            int imageTop = 62;
            int footerHeight = 35;
            int availableWidth = Math.max(1, width - 32);
            int availableHeight = Math.max(1, height - imageTop - footerHeight);
            if (image != null) {
                double scale = Math.min(
                    availableWidth / (double)image.getWidth(),
                    availableHeight / (double)image.getHeight());
                int drawWidth = Math.max(1, (int)Math.round(image.getWidth() * scale));
                int drawHeight = Math.max(1, (int)Math.round(image.getHeight() * scale));
                int drawX = (width - drawWidth) / 2;
                int drawY = imageTop + (availableHeight - drawHeight) / 2;
                g.drawImage(image, drawX, drawY, drawWidth, drawHeight, null);
            } else {
                g.setColor(new Color(255, 90, 70));
                g.drawString("COMPANION PNG OFFLINE", 20, height / 2);
            }

            g.setColor(GOLD);
            g.drawLine(16, height - 31, width - 16, height - 31);
            g.setFont(new Font(Font.MONOSPACED, Font.PLAIN, 10));
            g.drawString("OPERATOR " + operator.toUpperCase() + " // RGB32 PNG", 20, height - 15);
            g.dispose();
        }
    }

    private static BufferedImage loadImage(Path root, String filename) {
        for (Path directory : new Path[] {
                root.resolve("RES/WayKeeper TM"),
                Optional.ofNullable(root.getParent()).orElse(root).resolve("RES/WayKeeper TM")}) {
            Path candidate = directory.resolve(filename);
            if (Files.isRegularFile(candidate)) {
                try {
                    BufferedImage source = ImageIO.read(candidate.toFile());
                    return removeConnectedCheckerboard(source);
                } catch (IOException ignored) {
                    return null;
                }
            }
        }
        return null;
    }

    /* The supplied non-alpha PNGs contain a pale checkerboard baked into the border.
       Only pale neutral pixels connected to the outside edge are removed; enclosed white
       fur remains untouched behind the mascot's dark outline. */
    private static BufferedImage removeConnectedCheckerboard(BufferedImage source) {
        if (source == null || source.getColorModel().hasAlpha()) return source;
        int width = source.getWidth();
        int height = source.getHeight();
        BufferedImage result = new BufferedImage(width, height, BufferedImage.TYPE_INT_ARGB);
        int[] pixels = source.getRGB(0, 0, width, height, null, 0, width);
        boolean[] outside = new boolean[pixels.length];
        int[] queue = new int[pixels.length];
        int head = 0;
        int tail = 0;
        for (int x = 0; x < width; x++) {
            tail = enqueueBackground(x, pixels, outside, queue, tail);
            tail = enqueueBackground((height - 1) * width + x, pixels, outside, queue, tail);
        }
        for (int y = 0; y < height; y++) {
            tail = enqueueBackground(y * width, pixels, outside, queue, tail);
            tail = enqueueBackground(y * width + width - 1, pixels, outside, queue, tail);
        }
        while (head < tail) {
            int index = queue[head++];
            int x = index % width;
            int y = index / width;
            if (x > 0) tail = enqueueBackground(index - 1, pixels, outside, queue, tail);
            if (x + 1 < width) tail = enqueueBackground(index + 1, pixels, outside, queue, tail);
            if (y > 0) tail = enqueueBackground(index - width, pixels, outside, queue, tail);
            if (y + 1 < height) tail = enqueueBackground(index + width, pixels, outside, queue, tail);
        }
        for (int index = 0; index < pixels.length; index++) {
            pixels[index] = outside[index] ? pixels[index] & 0x00ffffff : pixels[index] | 0xff000000;
        }
        result.setRGB(0, 0, width, height, pixels, 0, width);
        return result;
    }

    private static int enqueueBackground(
            int index, int[] pixels, boolean[] outside, int[] queue, int tail) {
        if (!outside[index] && isCheckerPixel(pixels[index])) {
            outside[index] = true;
            queue[tail++] = index;
        }
        return tail;
    }

    private static boolean isCheckerPixel(int argb) {
        int red = (argb >>> 16) & 255;
        int green = (argb >>> 8) & 255;
        int blue = argb & 255;
        int maximum = Math.max(red, Math.max(green, blue));
        int minimum = Math.min(red, Math.min(green, blue));
        return minimum >= 225 && maximum - minimum <= 12;
    }

    private static String imageForIncident(String incident) {
        if (incident.contains("Nuclear") || incident.contains("Radiological")) return "VaultTec-Blue.png";
        if (incident.contains("Zombie")) return "Zombie-FalloutMode.png";
        return "SurvivalMode.png";
    }

    private static String modeLabel(String incident) {
        if (incident.contains("Nuclear") || incident.contains("Radiological")) return "NUCLEAR // VAULT-BLUE MODE";
        if (incident.contains("Zombie")) return "ZOMBIE // FALLOUT MODE";
        return "GENERAL SURVIVAL MODE";
    }

    private static boolean companionEnabled(Path settings) {
        Map<String, String> values = readIni(settings);
        return !"0".equals(values.getOrDefault("java_companion", "1")) &&
            !"minimal".equals(values.getOrDefault("layout_mode", "workstation"));
    }

    private static Map<String, String> readIni(Path path) {
        Map<String, String> values = new HashMap<>();
        try {
            for (String line : Files.readAllLines(path)) {
                int delimiter = line.indexOf('=');
                if (delimiter > 0) values.put(line.substring(0, delimiter), line.substring(delimiter + 1));
            }
        } catch (IOException ignored) {
            // Defaults are deliberately sufficient for first boot.
        }
        return values;
    }

    private static boolean acquireLock(Path settings) {
        try {
            Path directory = Optional.ofNullable(settings.getParent()).orElse(Path.of("."));
            Files.createDirectories(directory);
            lockChannel = FileChannel.open(directory.resolve("java-companion.lock"),
                StandardOpenOption.CREATE, StandardOpenOption.WRITE);
            processLock = lockChannel.tryLock();
            return processLock != null;
        } catch (IOException exception) {
            return false;
        }
    }

    private static void releaseLock() {
        try { if (processLock != null) processLock.release(); } catch (IOException ignored) {}
        try { if (lockChannel != null) lockChannel.close(); } catch (IOException ignored) {}
    }

    private static Map<String, String> parseArguments(String[] arguments) {
        Map<String, String> options = new HashMap<>();
        for (int index = 0; index + 1 < arguments.length; index += 2) {
            options.put(arguments[index].replaceFirst("^--", ""), arguments[index + 1]);
        }
        return options;
    }

    private static long parseLong(String value, long fallback) {
        try { return Long.parseLong(value); } catch (RuntimeException ignored) { return fallback; }
    }
}

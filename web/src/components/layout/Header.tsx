import { useQueryClient } from '@tanstack/react-query';
import { LogOut, Moon, Sun } from 'lucide-react';
import { Button } from '@/components/ui/button';
import { useTheme } from '@/hooks/useTheme';
import { clearAccessToken } from '@/lib/session';

export function Header() {
  const { theme, toggleTheme } = useTheme();
  const queryClient = useQueryClient();

  const signOut = () => {
    clearAccessToken();
    // What one token was allowed to read must not show for the next one.
    queryClient.clear();
  };

  return (
    <header className="flex h-16 items-center justify-between border-b bg-card px-6">
      <div className="flex items-center">
        <h1 className="text-xl font-semibold">PACS Administration</h1>
      </div>
      <div className="flex items-center space-x-4">
        <Button variant="ghost" size="icon" onClick={toggleTheme} aria-label="Toggle theme">
          {theme === 'dark' ? (
            <Sun className="h-5 w-5" />
          ) : (
            <Moon className="h-5 w-5" />
          )}
        </Button>
        <Button variant="ghost" size="icon" onClick={signOut} aria-label="Sign out">
          <LogOut className="h-5 w-5" />
        </Button>
      </div>
    </header>
  );
}

#include <string>

namespace NRpc::NDetail {

////////////////////////////////////////////////////////////////////////////////

inline const std::string NotFoundPage = "<!DOCTYPE html> \
<html lang=\"en\"> \
<head> \
    <meta charset=\"UTF-8\"> \
    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"> \
    <title>404 - Page Not Found</title> \
    <link rel=\"stylesheet\" href=\"/assets/style.css\"> \
</head> \
<body> \
    <div class=\"header\"> \
        <h1>Temperature Monitoring System</h1> \
        <p>Oops! Something went wrong.</p> \
    </div> \
    <div class=\"error-container\"> \
        <h2 class=\"error-code\">404</h2> \
        <p class=\"error-message\">Page Not Found</p> \
        <p class=\"error-details\"> \
            The page you are looking for might have been removed,  \
            had its name changed, or is temporarily unavailable. \
        </p> \
        <div class=\"card-footer\"> \
            <a href=\"/\" class=\"btn\">Back to Dashboard</a> \
        </div> \
    </div> \
</body> \
</html>";

////////////////////////////////////////////////////////////////////////////////

} // namespace NRpc::NDetail
